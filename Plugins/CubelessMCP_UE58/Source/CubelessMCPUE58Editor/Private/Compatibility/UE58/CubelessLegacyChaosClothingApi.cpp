#include "Compatibility/UE58/CubelessLegacyChaosClothingApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ClothLODData.h"
#include "ClothingAsset.h"
#include "ClothingAssetFactoryInterface.h"
#include "ClothingSystemEditorInterfaceModule.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Rendering/SkeletalMeshModel.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SkeletalMeshTypes.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::LegacyChaosClothing
{
namespace
{
using Cubeless::UE58::Variable::FResult;
constexpr int32 MaxJsonChars=1048576,MaxDepth=64,MaxSlots=262144;
bool Running=false;
bool IsPlaySessionActive() { return GEditor&&(GEditor->PlayWorld||GEditor->GetPIEWorldContext()); }
bool Live(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
FString Json(const TSharedPtr<FJsonObject>& O) { FString S;if(O) { FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S)); }return S; }
bool Dependency(const FString& V)
{
    FString S=FPackageName::ExportTextPathToObjectPath(V).TrimStartAndEnd();S.TrimQuotesInline();S.ReplaceInline(TEXT("\\"),TEXT("/"));
    for(const TCHAR* K:{TEXT("/Script/UnrealMCP"),TEXT("/UnrealMCP"),TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("CubelessControlRigTools"),TEXT("CubelessMCP_UE58"),TEXT("CubelessMCPUE58")}) { if(S.Contains(K,ESearchCase::IgnoreCase)) { return true; } }return false;
}
const TSharedPtr<FJsonValue>* FindJson(const TSharedPtr<FJsonObject>& O,const FString& Name)
{
    if(O) { for(const auto& Row:O->Values) { if(FString(Row.Key).Equals(Name,ESearchCase::CaseSensitive)) { return &Row.Value; } } }return nullptr;
}
struct FExactKeyFuncs:DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& S) { return FCrc::StrCrc32(*S); }
};
bool Parse(const FString& S,TSharedPtr<FJsonObject>& Out)
{
    if(S.IsEmpty()||S.Len()>MaxJsonChars) { return false; }for(TCHAR C:S) { if(C==0) { return false; } }
    auto Reader=TJsonReaderFactory<>::Create(S);TArray<bool> Objects;TArray<TSet<FString,FExactKeyFuncs>> Keys;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>MaxSlots) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()) { const FString K(Reader->GetIdentifier());if(Keys.Last().Contains(K)) { return false; }Keys.Last().Add(K); }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart) { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>MaxDepth) { return false; } }
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd) { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }
        if(Token==EJsonNotation::Number&&!FMath::IsFinite(Reader->GetValueAsNumber())) { return false; }
    }return Objects.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),Out)&&Out.IsValid();
}
struct FOwned
{
    TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Weak;TWeakObjectPtr<UPackage> Package;TWeakObjectPtr<UClass> Class;
    FString Path,PackagePath;bool bReleasedForCleanup=false;
};
struct FChild
{
    TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Weak,Outer,ExpectedParent;TWeakObjectPtr<UPackage> Package;TWeakObjectPtr<UClass> Class;FString Path,PackagePath;
};
struct FWork
{
    FResult R;TSharedPtr<FJsonObject> Params,BeforeSource;
    TSharedRef<FJsonObject> Calls=MakeShared<FJsonObject>(),Checks=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> SaveRows,CleanupRows;
    TStrongObjectPtr<USkeletalMesh> Source{nullptr};TWeakObjectPtr<UPackage> SourcePackage;TWeakObjectPtr<UClass> SourceClass;
    TStrongObjectPtr<UPhysicsAsset> SourcePhysics{nullptr};TStrongObjectPtr<UClothingAssetFactoryBase> Factory{nullptr};
    TArray<TStrongObjectPtr<UMaterialInterface>> Materials;TArray<FChild> Children;TArray<FOwned> Owned;TUniquePtr<FScopedTransaction> Transaction;
    bool CleanupTried=false;int32 CleanupRequested=0,CleanupDeleted=0;bool CleanupSucceeded=false;
    FString SourcePath,SourcePackagePath,FailedStage,OutputPath,OutputPhysicsPath;
    TWeakObjectPtr<UWorld> World;bool WorldCaptured=false,SourceCaptured=false,bWriting=false,bCleaning=false,bOuterTransaction=false;
    explicit FWork()
    {
        R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Calls);R.Data->SetObjectField(TEXT("postconditions"),Checks);R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Prefix);
        R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("explicit_wrapper_wait_performed"),false);
        R.Data->SetStringField(TEXT("compile_scope"),TEXT("No explicit Blueprint compiler. Skeletal post-edit/rebuild, mask updates, physics cooking and factory work are separate counted SDK effects and may synchronously compile/wait."));
        R.Data->SetStringField(TEXT("saved_flag_scope"),TEXT("saved retains any explicit wrapper singleton SavePackages SDK true result. DuplicateAsset can perform indirect SourceControl save/copy calls whose success is unknown. Clean readback, all requested packages, legacy vacuous save flags and current disk state are separate observations."));
        R.Data->SetStringField(TEXT("save_attempted_scope"),TEXT("save_attempted/save_calls count explicit wrapper SavePackages only. save_assets=false and zero wrapper calls do not guarantee no indirect SDK disk writes."));R.Data->SetBoolField(TEXT("no_disk_writes_guaranteed"),false);
        R.Data->SetStringField(TEXT("cleanup_scope"),TEXT("Only captured new output objects at their exact package/path are candidates. Public reference-checked single-object deletion and exact package cleanup can invoke modal/Undo/GC/SourceControl hooks. Residual outputs and incomplete cleanup remain explicit."));
        R.Data->SetBoolField(TEXT("runtime_cloth_simulation_verified"),false);R.Data->SetBoolField(TEXT("render_frame_completion_verified"),false);R.Data->SetBoolField(TEXT("save_reload_verified"),false);R.Data->SetBoolField(TEXT("whole_external_pipeline_complete"),false);
        for(const TCHAR* K:{TEXT("source_load_calls"),TEXT("material_load_calls"),TEXT("source_projection_calls"),TEXT("factory_calls"),TEXT("factory_returns"),TEXT("duplicate_calls"),TEXT("duplicate_returns"),TEXT("explicit_modify_calls"),TEXT("explicit_new_config_calls"),TEXT("physics_cook_calls"),TEXT("parameter_mask_calls"),TEXT("bind_calls"),TEXT("bind_returns"),TEXT("post_edit_scope_entries"),TEXT("post_edit_scope_returns"),TEXT("save_calls"),TEXT("save_returns"),TEXT("cleanup_delete_calls"),TEXT("cleanup_package_calls"),TEXT("explicit_Blueprint_compile_calls"),TEXT("explicit_wrapper_wait_calls")}) { Calls->SetNumberField(K,0); }
    }
    void Count(const TCHAR* K) { double N=0;Calls->TryGetNumberField(K,N);Calls->SetNumberField(K,N+1); }
    void SDK(const TCHAR* S) { R.Stage=S;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true); }
    void Write(const TCHAR* S) { SDK(S);bWriting=true;R.bMutationAttempted=true; }
    bool Fail(const TCHAR* Code,const FString& Error) { if(R.Error.IsEmpty()) { R.ErrorCode=TEXT("legacy_chaos_clothing_")+FString(Code);R.Error=Error;FailedStage=R.Stage;R.Data->SetStringField(TEXT("failed_stage"),FailedStage); }return false; }
    bool Guard()
    {
        if(!IsInGameThread()||!GEditor) { return Fail(TEXT("editor_lost"),TEXT("Editor game-thread context is unavailable")); }
        if(bCleaning&&GEditor->IsTransactionActive()) { return Fail(TEXT("cleanup_external_transaction_active"),TEXT("Cleanup stopped because an Editor transaction is currently active")); }
        if(WorldCaptured&&GEditor->GetEditorWorldContext().World()!=World.Get()) { return Fail(TEXT("world_changed"),TEXT("Captured Editor world changed")); }
        if(bWriting&&(GEditor->PlayWorld||GEditor->GetPIEWorldContext())) { return Fail(TEXT("PIE_started"),TEXT("PIE/SIE became active after writer admission")); }
        if(SourceCaptured&&(!Live(Source.Get())||Source->GetClass()!=SourceClass.Get()||Source->GetPackage()!=SourcePackage.Get()||!Source->GetPathName().Equals(SourcePath,ESearchCase::CaseSensitive)||!Source->GetPackage()->GetName().Equals(SourcePackagePath,ESearchCase::CaseSensitive))) { return Fail(TEXT("source_identity_lost"),TEXT("Captured source mesh/class/package identity changed")); }
        if(SourcePhysics.Get()!=nullptr&&!Live(SourcePhysics.Get())) { return Fail(TEXT("source_physics_lost"),TEXT("Captured source Physics Asset is no longer live")); }
        if(Factory.Get()!=nullptr&&!Live(Factory.Get())) { return Fail(TEXT("factory_lost"),TEXT("Captured factory provider is no longer live")); }
        for(const auto& M:Materials) { if(!Live(M.Get())) { return Fail(TEXT("render_material_lost"),TEXT("Captured render material is no longer live")); } }
        if(!bCleaning) { for(const auto& Child:Children) { const UObject* Object=Child.Weak.Get();if(!Live(Object)||!Live(Child.Package.Get())||!Live(Child.ExpectedParent.Get())||Object->GetClass()!=Child.Class.Get()||Object->GetOuter()!=Child.Outer.Get()||Object->GetPackage()!=Child.Package.Get()||!Object->IsIn(Child.ExpectedParent.Get())||!Object->GetPathName().Equals(Child.Path,ESearchCase::CaseSensitive)||!Child.Package->GetName().Equals(Child.PackagePath,ESearchCase::CaseSensitive)) { return Fail(TEXT("created_child_identity_lost"),TEXT("Captured created child class/outer/package/path or expected owned parent changed")); } }for(const auto& O:Owned) { if(!Live(O.Weak.Get())||O.Weak->GetClass()!=O.Class.Get()||O.Weak->GetPackage()!=O.Package.Get()||!O.Weak->GetPathName().Equals(O.Path,ESearchCase::CaseSensitive)||!O.Package->GetName().Equals(O.PackagePath,ESearchCase::CaseSensitive)) { return Fail(TEXT("output_identity_lost"),TEXT("Captured created output identity changed")); } } }
        return true;
    }
    bool OwnChild(UObject* Object,UObject* ExpectedParent)
    {
        if(!Live(Object)||!Live(ExpectedParent)||!Live(Object->GetPackage())||!Object->IsIn(ExpectedParent)||Dependency(Object->GetClass()->GetPathName())) { return Fail(TEXT("unproven_child_ownership"),TEXT("Created child is not a live native object under its expected owned parent")); }
        auto& Child=Children.AddDefaulted_GetRef();Child.Hold.Reset(Object);Child.Weak=Object;Child.Class=Object->GetClass();Child.Outer=Object->GetOuter();Child.ExpectedParent=ExpectedParent;Child.Package=Object->GetPackage();Child.Path=Object->GetPathName();Child.PackagePath=Object->GetPackage()->GetName();return Guard();
    }
    void ObserveChildren(const TCHAR* Key)
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(const auto& Child:Children)
        {
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("captured_path"),Child.Path);Row->SetStringField(TEXT("captured_package"),Child.PackagePath);const UObject* Object=Child.Weak.Get();Row->SetBoolField(TEXT("object_live"),Live(Object));
            if(Live(Object)) { Row->SetStringField(TEXT("current_path"),Object->GetPathName());Row->SetStringField(TEXT("current_package"),Object->GetPackage()->GetName());Row->SetBoolField(TEXT("captured_class_outer_package_unchanged"),Object->GetClass()==Child.Class.Get()&&Object->GetOuter()==Child.Outer.Get()&&Object->GetPackage()==Child.Package.Get());Row->SetBoolField(TEXT("still_within_expected_parent"),Live(Child.ExpectedParent.Get())&&Object->IsIn(Child.ExpectedParent.Get())); }
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        R.Data->SetArrayField(Key,Rows);
    }
    bool Own(UObject* Object,const FString& ExpectedObjectPath)
    {
        if(!Live(Object)||!Live(Object->GetPackage())||!Object->GetPathName().Equals(ExpectedObjectPath,ESearchCase::CaseSensitive)||Dependency(Object->GetClass()->GetPathName())) { return Fail(TEXT("unproven_output_ownership"),TEXT("SDK did not return a live native output at the exact admitted path; automatic deletion is not authorized for an unproven object")); }
        auto& O=Owned.AddDefaulted_GetRef();O.Hold.Reset(Object);O.Weak=Object;O.Package=Object->GetPackage();O.Class=Object->GetClass();O.Path=Object->GetPathName();O.PackagePath=Object->GetPackage()->GetName();Prefix->SetNumberField(TEXT("owned_top_level_output_count"),Owned.Num());SDK(TEXT("observe_returned_owned_duplicate"));auto Created=MakeShared<FJsonObject>();Created->SetStringField(TEXT("path"),O.Path);Created->SetBoolField(TEXT("package_dirty"),O.Package->IsDirty());Created->SetBoolField(TEXT("package_file_present_after_duplicate"),FPackageName::DoesPackageExist(O.PackagePath));Prefix->SetObjectField(TEXT("latest_created_output_observation"),Created);R.bChanged=true;return Guard();
    }
    TSharedPtr<FJsonObject> SourceProjection()
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("source_live"),Live(Source.Get()));if(!Live(Source.Get())) { return J; }SDK(TEXT("observe_source_mesh"));Count(TEXT("source_projection_calls"));
        J->SetStringField(TEXT("path"),Source->GetPathName());J->SetStringField(TEXT("class"),Source->GetClass()->GetPathName());J->SetStringField(TEXT("package"),Source->GetPackage()->GetName());J->SetBoolField(TEXT("package_dirty"),Source->GetPackage()->IsDirty());
        J->SetStringField(TEXT("physics_asset"),Source->GetPhysicsAsset()?Source->GetPhysicsAsset()->GetPathName():FString());TArray<TSharedPtr<FJsonValue>> MaterialRows;
        for(const auto& M:Source->GetMaterials()) { auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("slot"),M.MaterialSlotName.ToString());P->SetStringField(TEXT("material"),M.MaterialInterface?M.MaterialInterface->GetPathName():FString());MaterialRows.Add(MakeShared<FJsonValueObject>(P)); }J->SetArrayField(TEXT("materials"),MaterialRows);
        TArray<TSharedPtr<FJsonValue>> ClothingRows;for(const auto& A:Source->GetMeshClothingAssets()) { ClothingRows.Add(MakeShared<FJsonValueString>(A?A->GetPathName():FString())); }J->SetArrayField(TEXT("clothing_assets"),ClothingRows);
        const auto* Model=Source->GetImportedModel();if(!Guard()) { return J; }J->SetBoolField(TEXT("imported_lod0_present"),Model&&Model->LODModels.IsValidIndex(0));
        if(Model&&Model->LODModels.IsValidIndex(0))
        {
            const auto& Lod=Model->LODModels[0];FMD5 H;TArray<TSharedPtr<FJsonValue>> Sections;
            for(const auto& S:Lod.Sections)
            {
                auto P=MakeShared<FJsonObject>();P->SetNumberField(TEXT("material_index"),S.MaterialIndex);P->SetNumberField(TEXT("original_section_index"),S.OriginalDataSectionIndex);P->SetNumberField(TEXT("vertex_count"),S.NumVertices);P->SetNumberField(TEXT("triangle_count"),S.NumTriangles);P->SetStringField(TEXT("cloth_guid"),S.ClothingData.AssetGuid.ToString());P->SetNumberField(TEXT("cloth_lod"),S.ClothingData.AssetLodIndex);P->SetBoolField(TEXT("cloth_mapping_present"),S.HasClothingData());Sections.Add(MakeShared<FJsonValueObject>(P));
                for(const auto& V:S.SoftVertices) { H.Update(reinterpret_cast<const uint8*>(&V.Position),sizeof(V.Position));H.Update(reinterpret_cast<const uint8*>(&V.Color),sizeof(V.Color)); }
            }if(!Lod.IndexBuffer.IsEmpty()) { H.Update(reinterpret_cast<const uint8*>(Lod.IndexBuffer.GetData()),static_cast<uint32>(Lod.IndexBuffer.Num()*sizeof(uint32))); }uint8 D[16];H.Final(D);J->SetStringField(TEXT("lod0_positions_colors_indices_md5"),BytesToHex(D,UE_ARRAY_COUNT(D)).ToLower());J->SetArrayField(TEXT("sections"),Sections);
        }
        const auto& Ref=Source->GetRefSkeleton();FMD5 BoneHash;
        for(int32 I=0;I<Ref.GetNum();++I)
        {
            const FTCHARToUTF8 Name(*Ref.GetBoneName(I).ToString());BoneHash.Update(reinterpret_cast<const uint8*>(Name.Get()),Name.Length());const uint8 Separator=0;BoneHash.Update(&Separator,1);const int32 Parent=Ref.GetParentIndex(I);BoneHash.Update(reinterpret_cast<const uint8*>(&Parent),sizeof(Parent));
            const auto& Pose=Ref.GetRefBonePose()[I];const auto T=Pose.GetTranslation(),S=Pose.GetScale3D();const auto Q=Pose.GetRotation();const double Values[]={T.X,T.Y,T.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z};BoneHash.Update(reinterpret_cast<const uint8*>(Values),sizeof(Values));
        }
        uint8 BoneDigest[16];BoneHash.Final(BoneDigest);J->SetStringField(TEXT("ref_skeleton_names_parents_local_poses_md5"),BytesToHex(BoneDigest,UE_ARRAY_COUNT(BoneDigest)).ToLower());
        J->SetStringField(TEXT("projection_scope"),TEXT("captured loaded mesh identity/dirty/material paths/physics reference/clothing paths/reference-skeleton names parents local poses/LOD0 section binding geometry positions colors and indices; not byte-exact package or all derived/runtime state"));return J;
    }
    FString Finish(const TSharedPtr<FJsonObject>& Result)
    {
        auto Legacy=Result.IsValid()?Result:MakeShared<FJsonObject>();bool Success=false;Legacy->TryGetBoolField(TEXT("success"),Success);
        if(SourceCaptured) { auto After=SourceProjection();R.Data->SetObjectField(TEXT("source_after"),After);const bool Same=BeforeSource.IsValid()&&Json(BeforeSource).Equals(Json(After),ESearchCase::CaseSensitive);Checks->SetBoolField(TEXT("captured_source_projection_unchanged"),Same);if(!Same) { R.bChanged=true;Fail(TEXT("source_projection_changed"),TEXT("Captured source projection changed; only the observed scope is known and automatic source restoration is not performed")); } }
        bool Changed=false;Legacy->TryGetBoolField(TEXT("changed"),Changed);R.bChanged|=Changed;
        if(!Success&&R.Error.IsEmpty()) { FString Error;Legacy->TryGetStringField(TEXT("error"),Error);Fail(TEXT("operation_failed"),Error.IsEmpty()?TEXT("Legacy cloth creation failed"):Error); }
        if(Success&&!R.Error.IsEmpty())
        {
            Legacy->SetBoolField(TEXT("legacy_success_before_terminal_guard"),true);Legacy->SetBoolField(TEXT("success"),false);Legacy->SetBoolField(TEXT("verified_success"),false);Legacy->SetBoolField(TEXT("do_not_retry"),R.bMutationAttempted||!R.bChangeStateKnown);Legacy->SetStringField(TEXT("error"),R.Error);Legacy->SetStringField(TEXT("error_code"),R.ErrorCode);Legacy->SetStringField(TEXT("success_semantics"),TEXT("postcondition_failed"));
            const TSharedPtr<FJsonObject>* Verification=nullptr;if(Legacy->TryGetObjectField(TEXT("verification"),Verification)&&Verification&&Verification->IsValid()) { (*Verification)->SetStringField(TEXT("status"),TEXT("failed"));(*Verification)->SetBoolField(TEXT("native_terminal_guard_passed"),false); }
        }
        ObserveChildren(TEXT("current_captured_child_observations"));TArray<TSharedPtr<FJsonValue>> CurrentOutputs;
        for(const auto& O:Owned)
        {
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("captured_path"),O.Path);Row->SetStringField(TEXT("captured_package"),O.PackagePath);const UObject* Object=O.Weak.Get();const UPackage* Package=O.Package.Get();Row->SetBoolField(TEXT("object_live"),Live(Object));Row->SetBoolField(TEXT("captured_package_live"),Live(Package));
            if(Live(Object)) { Row->SetStringField(TEXT("current_object_path"),Object->GetPathName());Row->SetBoolField(TEXT("current_object_is_asset"),Object->IsAsset());Row->SetBoolField(TEXT("captured_object_identity_unchanged"),Object->GetClass()==O.Class.Get()&&Object->GetPackage()==Package&&Object->GetPathName().Equals(O.Path,ESearchCase::CaseSensitive)); }
            if(Live(Package)) { Row->SetStringField(TEXT("current_captured_package_path"),Package->GetName());Row->SetBoolField(TEXT("current_captured_package_dirty"),Package->IsDirty());Row->SetBoolField(TEXT("captured_package_name_unchanged"),Package->GetName().Equals(O.PackagePath,ESearchCase::CaseSensitive)); }
            SDK(TEXT("observe_exact_output_prefix"));Row->SetBoolField(TEXT("package_file_exists"),FPackageName::DoesPackageExist(O.PackagePath));Row->SetBoolField(TEXT("asset_registry_entry_present"),FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(O.Path)).IsValid());CurrentOutputs.Add(MakeShared<FJsonValueObject>(Row));
        }
        R.bSuccess=Success&&R.Error.IsEmpty();R.bPostconditionPassed=R.bSuccess;R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetArrayField(TEXT("actual_save_prefix"),SaveRows);R.Data->SetArrayField(TEXT("actual_cleanup_prefix"),CleanupRows);R.Data->SetArrayField(TEXT("current_owned_output_observations"),CurrentOutputs);R.Data->SetBoolField(TEXT("global_change_state_unknown"),!R.bChangeStateKnown);R.Data->SetBoolField(TEXT("cleanup_may_invalidate_Undo_history"),bCleaning);
        if(R.bSuccess) { R.Stage=TEXT("complete"); }else if(!FailedStage.IsEmpty()) { R.Stage=FailedStage; }return Cubeless::Operations::EncodeVariableResult(R,TEXT("create_legacy_chaos_clothing_from_skeletal_sections"));
    }
};
FWork* Work=nullptr;
bool Guard() { return !Work||Work->Guard(); }
void SDK(const TCHAR* S) { if(Work) { Work->SDK(S); } }
void Count(const TCHAR* S) { if(Work) { Work->Count(S); } }
struct FObservedPostEditScope
{
    TUniquePtr<FScopedSkeletalMeshPostEditChange> Scope;
    explicit FObservedPostEditScope(USkeletalMesh* Mesh) { SDK(TEXT("skeletal_post_edit_scope"));Count(TEXT("post_edit_scope_entries"));Scope=MakeUnique<FScopedSkeletalMeshPostEditChange>(Mesh); }
    ~FObservedPostEditScope() { Scope.Reset();Count(TEXT("post_edit_scope_returns")); }
};
bool AdmitFreshOutput(const FString& PackagePath,const FString& ObjectPath)
{
    SDK(TEXT("recheck_exact_output_before_duplicate"));const bool Exists=FindObject<UObject>(nullptr,*ObjectPath)||FindPackage(nullptr,*PackagePath)||FPackageName::DoesPackageExist(PackagePath)||FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(ObjectPath)).IsValid();
    if(!Guard()) { return false; }if(Exists) { return Work->Fail(TEXT("output_collision_before_duplicate"),TEXT("Output appeared after preflight; no duplicate/overwrite call was made for this target")); }return true;
}
TSharedPtr<FJsonObject> ErrorResponse(const FString& E)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("status"),TEXT("error"));J->SetStringField(TEXT("error"),E);
    if(Work) { Work->Fail(TEXT("operation_failed"),E);J->SetBoolField(TEXT("mutation_attempted"),Work->R.bMutationAttempted);J->SetBoolField(TEXT("partial_target_modified"),Work->R.bMutationAttempted);J->SetBoolField(TEXT("do_not_retry"),Work->R.bMutationAttempted||!Work->R.bChangeStateKnown);J->SetBoolField(TEXT("save_requested"),Work->R.bSaveRequested);J->SetBoolField(TEXT("save_attempted"),Work->R.bSaveAttempted);J->SetStringField(TEXT("output_skeletal_mesh_path"),Work->OutputPath); }return J;
}
struct FLegacy
{
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& E) { return ErrorResponse(E); }
    static void AddMutationResultContract(const TSharedPtr<FJsonObject>& Result,bool bDryRun,bool bMutationPerformed,bool bChanged,const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,const TSharedPtr<FJsonObject>& Delta,bool bVerificationPassed,const FString& VerificationMethod,const TArray<FString>& VerificationChecks,bool bStrictSuccess=true);
};
void FLegacy::AddMutationResultContract(
    const TSharedPtr<FJsonObject>& Result,
    bool bDryRun,
    bool bMutationPerformed,
    bool bChanged,
    const TSharedPtr<FJsonObject>& Before,
    const TSharedPtr<FJsonObject>& After,
    const TSharedPtr<FJsonObject>& Delta,
    bool bVerificationPassed,
    const FString& VerificationMethod,
    const TArray<FString>& VerificationChecks,
    bool bStrictSuccess)
{
    if (!Result.IsValid())
    {
        return;
    }

    bool bLegacySuccess = true;
    if (Result->HasTypedField<EJson::Boolean>(TEXT("success")))
    {
        bLegacySuccess = Result->GetBoolField(TEXT("success"));
    }

    Result->SetStringField(TEXT("contract_version"), TEXT("mutation_result_v1"));
    Result->SetStringField(TEXT("mode"), bDryRun ? TEXT("dry_run") : TEXT("apply"));
    Result->SetBoolField(TEXT("dry_run"), bDryRun);
    Result->SetBoolField(TEXT("DRY_RUN"), bDryRun);
    Result->SetBoolField(TEXT("changed"), bChanged);
    Result->SetBoolField(TEXT("mutation_performed"), bMutationPerformed);

    TSharedPtr<FJsonObject> Mutation = MakeShared<FJsonObject>();
    Mutation->SetBoolField(TEXT("attempted"), !bDryRun);
    Mutation->SetBoolField(TEXT("performed"), bMutationPerformed);
    Mutation->SetStringField(
        TEXT("state"),
        bMutationPerformed ? TEXT("performed") : TEXT("not_performed"));
    Result->SetObjectField(TEXT("mutation"), Mutation);

    if (Before.IsValid())
    {
        Result->SetObjectField(TEXT("before"), Before);
    }
    if (After.IsValid())
    {
        Result->SetObjectField(TEXT("after"), After);
    }

    TSharedPtr<FJsonObject> NormalizedDelta = Delta.IsValid()
        ? Delta
        : MakeShared<FJsonObject>();
    static const TCHAR* CanonicalCountFields[] =
    {
        TEXT("affected_count"),
        TEXT("created_count"),
        TEXT("updated_count"),
        TEXT("removed_count"),
        TEXT("skipped_count"),
        TEXT("failed_count")
    };
    for (const TCHAR* FieldName : CanonicalCountFields)
    {
        if (!NormalizedDelta->HasField(FieldName))
        {
            NormalizedDelta->SetField(FieldName, MakeShared<FJsonValueNull>());
        }
    }
    Result->SetObjectField(TEXT("delta"), NormalizedDelta);

    TSharedPtr<FJsonObject> Verification = MakeShared<FJsonObject>();
    Verification->SetStringField(
        TEXT("status"),
        bVerificationPassed ? TEXT("passed") : TEXT("failed"));
    Verification->SetStringField(TEXT("method"), VerificationMethod);
    Verification->SetBoolField(TEXT("before_observed"), Before.IsValid());
    Verification->SetBoolField(TEXT("after_observed"), After.IsValid());
    Verification->SetBoolField(TEXT("strict"), bStrictSuccess);
    TArray<TSharedPtr<FJsonValue>> CheckValues;
    CheckValues.Reserve(VerificationChecks.Num());
    for (const FString& Check : VerificationChecks)
    {
        CheckValues.Add(MakeShared<FJsonValueString>(Check));
    }
    Verification->SetArrayField(TEXT("checks"), CheckValues);
    Result->SetObjectField(TEXT("verification"), Verification);

    const bool bVerifiedSuccess = bLegacySuccess && bVerificationPassed;
    Result->SetBoolField(TEXT("verified_success"), bVerifiedSuccess);
    FString SuccessSemantics = TEXT("legacy_execution");
    if (bDryRun)
    {
        SuccessSemantics = bVerifiedSuccess ? TEXT("verified_dry_run") : TEXT("failed_dry_run");
    }
    else if (bVerifiedSuccess)
    {
        SuccessSemantics = TEXT("verified_postcondition");
    }
    else if (bStrictSuccess && bLegacySuccess)
    {
        SuccessSemantics = TEXT("postcondition_failed");
    }
    else if (!bLegacySuccess)
    {
        SuccessSemantics = TEXT("failed_execution");
    }
    Result->SetStringField(TEXT("success_semantics"), SuccessSemantics);

    if (!Result->HasField(TEXT("success")))
    {
        Result->SetBoolField(TEXT("success"), bLegacySuccess);
    }
    if (bStrictSuccess && bLegacySuccess && !bVerificationPassed)
    {
        Result->SetBoolField(TEXT("legacy_success"), true);
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error_code"), TEXT("mutation_postcondition_failed"));
        if (!Result->HasField(TEXT("error")))
        {
            Result->SetStringField(
                TEXT("error"),
                TEXT("Mutation postcondition failed independent readback"));
        }
    }
}
struct FJsonParamParseState
{
    void AddError(const FString& Error)
    {
        Errors.Add(Error);
    }

    bool IsValid() const
    {
        return Errors.IsEmpty();
    }

    FString GetErrorMessage() const
    {
        return FString::Printf(TEXT("Invalid Chaos tuning parameters: %s"), *FString::Join(Errors, TEXT("; ")));
    }

private:
    TArray<FString> Errors;
};

FString GetStringParam(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* Name,
    FJsonParamParseState& ParseState,
    const FString& Default = FString())
{
    if (!Params.IsValid())
    {
        return Default;
    }
    const TSharedPtr<FJsonValue>* JsonValue = FindJson(Params,Name);
    if (!JsonValue)
    {
        return Default;
    }
    FString Value;
    if (!JsonValue->IsValid() || (*JsonValue)->Type != EJson::String || !(*JsonValue)->TryGetString(Value))
    {
        ParseState.AddError(FString::Printf(TEXT("%s must be a JSON string"), Name));
        return Default;
    }
    return Value;
}

bool GetBoolParam(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* Name,
    bool bDefault,
    FJsonParamParseState& ParseState)
{
    if (!Params.IsValid())
    {
        return bDefault;
    }
    const TSharedPtr<FJsonValue>* JsonValue = FindJson(Params,Name);
    if (!JsonValue)
    {
        return bDefault;
    }
    bool bValue = bDefault;
    if (!JsonValue->IsValid() || (*JsonValue)->Type != EJson::Boolean || !(*JsonValue)->TryGetBool(bValue))
    {
        ParseState.AddError(FString::Printf(TEXT("%s must be a JSON boolean"), Name));
        return bDefault;
    }
    return bValue;
}

int32 GetIntParam(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* Name,
    int32 Default,
    FJsonParamParseState& ParseState)
{
    if (!Params.IsValid())
    {
        return Default;
    }
    const TSharedPtr<FJsonValue>* JsonValue = FindJson(Params,Name);
    if (!JsonValue)
    {
        return Default;
    }
    double Value = 0.0;
    if (!JsonValue->IsValid() || (*JsonValue)->Type != EJson::Number ||
        !(*JsonValue)->TryGetNumber(Value) || !FMath::IsFinite(Value) ||
        Value != FMath::TruncToDouble(Value) ||
        Value < static_cast<double>(TNumericLimits<int32>::Min()) ||
        Value > static_cast<double>(TNumericLimits<int32>::Max()))
    {
        ParseState.AddError(FString::Printf(
            TEXT("%s must be a JSON number containing a 32-bit signed integer"),
            Name));
        return Default;
    }
    return static_cast<int32>(Value);
}

bool ReadNumber(const TSharedPtr<FJsonValue>& Value, double& OutValue)
{
    return Value.IsValid() && Value->Type == EJson::Number && Value->TryGetNumber(OutValue);
}

bool ReadBool(const TSharedPtr<FJsonValue>& Value, bool& bOutValue)
{
    return Value.IsValid() && Value->Type == EJson::Boolean && Value->TryGetBool(bOutValue);
}

bool ReadString(const TSharedPtr<FJsonValue>& Value, FString& OutValue)
{
    return Value.IsValid() && Value->Type == EJson::String && Value->TryGetString(OutValue);
}

bool IsFiniteFloatRange(double Value)
{
    return FMath::IsFinite(Value) &&
        FMath::Abs(Value) <= static_cast<double>(TNumericLimits<float>::Max());
}
bool IsDurablySavablePackage(const UPackage* Package)
{
    if (!Package || Package == GetTransientPackage() ||
        Package->HasAnyPackageFlags(PKG_CompiledIn | PKG_PlayInEditor))
    {
        return false;
    }
    const FString PackageName = Package->GetName();
    return !FPackageName::IsMemoryPackage(PackageName) &&
        !FPackageName::IsTempPackage(PackageName) &&
        FPackageName::IsValidLongPackageName(PackageName, false);
}

bool IsDurablySavableObject(const UObject* Object)
{
    if (!Object)
    {
        return false;
    }
    for (const UObject* Outer = Object; Outer; Outer = Outer->GetOuter())
    {
        if (Outer->HasAnyFlags(RF_Transient))
        {
            return false;
        }
    }
    return IsDurablySavablePackage(Object->GetOutermost());
}

bool SaveExactPackage(UPackage* Package)
{
    if (!IsDurablySavablePackage(Package))
    {
        return false;
    }
    TArray<UPackage*> Packages{Package};
    if(!Guard()) { return false; }TStrongObjectPtr<UPackage> PackageHold(Package);const FString Before=Package->GetName();SDK(TEXT("save_exact_owned_package"));Count(TEXT("save_calls"));Work->R.bSaveAttempted=true;
    const bool Returned=UEditorLoadingAndSavingUtils::SavePackages(Packages,false);Count(TEXT("save_returns"));const bool Clean=Live(Package)&&!Package->IsDirty();const bool Saved=Returned&&Clean;Work->R.bSaved|=Returned;
    auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("captured_package_before"),Before);if(Live(Package)) { Row->SetStringField(TEXT("captured_package_after"),Package->GetName()); }else { Row->SetField(TEXT("captured_package_after"),MakeShared<FJsonValueNull>()); }
    Row->SetBoolField(TEXT("SDK_save_returned"),Returned);Row->SetBoolField(TEXT("package_clean_after"),Clean);Row->SetBoolField(TEXT("captured_package_name_unchanged"),Live(Package)&&Package->GetName().Equals(Before,ESearchCase::CaseSensitive));Work->SaveRows.Add(MakeShared<FJsonValueObject>(Row));const bool Identity=Guard();Row->SetBoolField(TEXT("captured_target_identity_after_save"),Identity);if(!Saved) { Work->Fail(TEXT("save_unverified"),TEXT("Exact owned package save returned false or clean readback failed")); }return Saved;
}

bool SaveObjectIfRequested(UObject* Object, bool bSaveRequested)
{
    return !bSaveRequested ||
        (IsDurablySavableObject(Object) && SaveExactPackage(Object->GetOutermost()));
}

bool HasMaterialUsageWithoutMutation(UMaterialInterface* MaterialInterface, EMaterialUsage Usage)
{
    const UMaterial* const BaseMaterial = MaterialInterface ? MaterialInterface->GetMaterial() : nullptr;
    if(!Guard()) { return false; }
    if (!Live(BaseMaterial))
    {
        return false;
    }
    bool bHasUsage = false;
    const bool bNeedsUsageUpdate = BaseMaterial->NeedsSetMaterialUsage_Concurrent(bHasUsage, Usage);
    return !bNeedsUsageUpdate && bHasUsage;
}

struct FOwnedAssetCleanupResult
{
    int32 RequestedCount=0,DeletedCount=0;bool bSucceeded=true;
};
FOwnedAssetCleanupResult DeleteOwnedAssetsWithoutConfirmation(const TArray<UObject*>&)
{
    FOwnedAssetCleanupResult Result;if(!Work) { Result.bSucceeded=false;return Result; }
    Result.RequestedCount=Work->Owned.Num();if(Work->CleanupTried) { Result.RequestedCount=Work->CleanupRequested;Result.DeletedCount=Work->CleanupDeleted;Result.bSucceeded=Work->CleanupSucceeded;return Result; }
    Work->CleanupTried=true;Work->CleanupRequested=Result.RequestedCount;Work->bCleaning=true;Work->Transaction.Reset();
    Work->Prefix->SetBoolField(TEXT("own_transaction_scope_closed_before_cleanup"),true);
    if(Work->bOuterTransaction||!Guard()) { Result.bSucceeded=false;Work->R.Data->SetStringField(TEXT("cleanup_incomplete_reason"),TEXT("Outer transaction or captured context is not safe for exact output cleanup; no delete was attempted"));return Result; }
    // Strong created-output references would themselves prevent checked deletion.
    // Public/standalone flags keep them alive until the exact deletion call.
    Work->ObserveChildren(TEXT("created_children_before_cleanup_release"));Work->Children.Reset();for(auto& O:Work->Owned) { O.Hold.Reset();O.bReleasedForCleanup=true; }
    for(auto& O:Work->Owned)
    {
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),O.Path);Row->SetStringField(TEXT("package"),O.PackagePath);Work->CleanupRows.Add(MakeShared<FJsonValueObject>(Row));
        UObject* Object=O.Weak.Get();const bool Exact=Live(Object)&&Object->GetClass()==O.Class.Get()&&Object->GetPackage()==O.Package.Get()&&Object->GetPathName().Equals(O.Path,ESearchCase::CaseSensitive)&&O.Package->GetName().Equals(O.PackagePath,ESearchCase::CaseSensitive);
        Row->SetBoolField(TEXT("exact_owned_identity_before_delete"),Exact);
        if(!Exact) { Row->SetStringField(TEXT("reason"),TEXT("captured owned identity is no longer proven"));continue; }
        bool Referenced=false,UndoReferenced=false;
        SDK(TEXT("cleanup_reference_admission"));ObjectTools::GatherObjectReferencersForDeletion(Object,Referenced,UndoReferenced);if(!Guard()) { break; }
        Row->SetBoolField(TEXT("object_referenced"),Referenced);Row->SetBoolField(TEXT("object_referenced_by_Undo"),UndoReferenced);
        // These SDK deletion methods can ResetTransaction for Undo-only targets.
        // Preserve the residual output rather than trade unrelated history for cleanup.
        if(Referenced||UndoReferenced) { Row->SetStringField(TEXT("reason"),TEXT("Reference or Undo state requires preserving residual output; force/unchecked/global history reset is not used"));continue; }
        Object=O.Weak.Get();const bool ExactAfterAdmission=Live(Object)&&Object->GetClass()==O.Class.Get()&&Object->GetPackage()==O.Package.Get()&&Object->GetPathName().Equals(O.Path,ESearchCase::CaseSensitive)&&Live(O.Package.Get())&&O.Package->GetName().Equals(O.PackagePath,ESearchCase::CaseSensitive);Row->SetBoolField(TEXT("exact_owned_identity_after_reference_admission"),ExactAfterAdmission);if(!ExactAfterAdmission) { Row->SetStringField(TEXT("reason"),TEXT("Owned identity changed during reference admission; destructive deletion was skipped"));continue; }
        SDK(TEXT("delete_exact_owned_object"));Count(TEXT("cleanup_delete_calls"));const bool Deleted=ObjectTools::DeleteSingleObject(Object,true);Row->SetBoolField(TEXT("DeleteSingleObject_returned"),Deleted);
        if(!Guard()) { Row->SetBoolField(TEXT("context_valid_after_object_delete"),false);break; }Row->SetBoolField(TEXT("context_valid_after_object_delete"),true);
        if(Deleted)
        {
            UPackage* Package=O.Package.Get();if(Live(Package)&&Package->GetName().Equals(O.PackagePath,ESearchCase::CaseSensitive))
            {
                bool PackageReferenced=false,PackageUndo=false;SDK(TEXT("cleanup_package_reference_admission"));ObjectTools::GatherObjectReferencersForDeletion(Package,PackageReferenced,PackageUndo);Row->SetBoolField(TEXT("package_referenced_after_object_delete"),PackageReferenced);Row->SetBoolField(TEXT("package_referenced_by_Undo_after_object_delete"),PackageUndo);if(!Guard()) { break; }
                Package=O.Package.Get();const bool ExactPackage=Live(Package)&&Package->GetName().Equals(O.PackagePath,ESearchCase::CaseSensitive);Row->SetBoolField(TEXT("captured_package_identity_after_reference_admission"),ExactPackage);
                if(!PackageReferenced&&!PackageUndo&&ExactPackage) { SDK(TEXT("cleanup_exact_owned_package"));Count(TEXT("cleanup_package_calls"));ObjectTools::CleanupAfterSuccessfulDelete({Package},true); }
                else { Row->SetStringField(TEXT("package_cleanup_skipped_reason"),TEXT("Remaining package reference or Undo state is preserved after exact object deletion")); }
            }
        }
        const bool FileExists=FPackageName::DoesPackageExist(O.PackagePath);
        const bool AssetRegistered=FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(O.Path)).IsValid();
        const bool StillPublic=O.Weak.IsValid()&&O.Weak->IsAsset();Row->SetBoolField(TEXT("package_file_exists_after"),FileExists);Row->SetBoolField(TEXT("asset_registered_after"),AssetRegistered);Row->SetBoolField(TEXT("public_asset_still_live_after"),StillPublic);
        const bool Removed=Deleted&&!FileExists&&!AssetRegistered&&!StillPublic;Row->SetBoolField(TEXT("cleanup_verified"),Removed);Result.DeletedCount+=Removed?1:0;
        if(!Guard()) { break; }
    }
    Result.bSucceeded=Result.DeletedCount==Result.RequestedCount;Work->CleanupDeleted=Result.DeletedCount;Work->CleanupSucceeded=Result.bSucceeded;return Result;
}
TSharedPtr<FJsonObject> CreateLegacy(
    const TSharedPtr<FJsonObject>& Params)
{
    struct FSectionRequest
    {
        int32 SectionIndex = INDEX_NONE;
        FString ExpectedMaterialSlot;
        FString MaterialPath;
        float MaxDistanceCm = 0.f;
        FString ActualMaterialSlot;
        UMaterialInterface* RenderMaterial = nullptr;
        FString MaterialSource;
        int32 ExpectedOriginalDataSectionIndex = INDEX_NONE;
        int32 VertexCount = 0;
        int32 TriangleCount = 0;
        int32 PinnedGuideVertexCount = 0;
        int32 DynamicGuideVertexCount = 0;
        bool bUseVerticalPinFallback = false;
        float VerticalPinFallbackThreshold = 0.f;
    };

    FJsonParamParseState ParamState;
    const FString SourceSkeletalMeshPath = GetStringParam(Params, TEXT("source_skeletal_mesh_path"), ParamState);
    FString OutputSkeletalMeshPath = GetStringParam(Params, TEXT("output_skeletal_mesh_path"), ParamState);
    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), true, ParamState);
    const bool bSaveAssets = GetBoolParam(Params, TEXT("save_assets"), true, ParamState);
    const bool bUseSourcePhysicsAsset = GetBoolParam(
        Params,
        TEXT("use_source_physics_asset"),
        true,
        ParamState);
    const bool bCreateVroidLowerBodyClothPhysicsAsset = GetBoolParam(
        Params,
        TEXT("create_vroid_lower_body_cloth_physics_asset"),
        false,
        ParamState);

    const TArray<TSharedPtr<FJsonValue>>* SectionValues = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(TEXT("section_configs"), SectionValues) || !SectionValues)
    {
        ParamState.AddError(TEXT("section_configs must be a JSON array"));
    }

    TArray<FSectionRequest> SectionRequests;
    if (SectionValues)
    {
        if (SectionValues->IsEmpty() || SectionValues->Num() > 8)
        {
            ParamState.AddError(TEXT("section_configs must contain between 1 and 8 garment sections"));
        }
        TSet<int32> RequestedSections;
        for (int32 ArrayIndex = 0; ArrayIndex < SectionValues->Num(); ++ArrayIndex)
        {
            const TSharedPtr<FJsonObject>* SectionObjectPtr = nullptr;
            if (!(*SectionValues)[ArrayIndex].IsValid() ||
                !(*SectionValues)[ArrayIndex]->TryGetObject(SectionObjectPtr) ||
                !SectionObjectPtr || !SectionObjectPtr->IsValid())
            {
                ParamState.AddError(FString::Printf(TEXT("section_configs[%d] must be an object"), ArrayIndex));
                continue;
            }

            const TSharedPtr<FJsonObject>& SectionObject = *SectionObjectPtr;
            FSectionRequest Request;
            Request.SectionIndex = GetIntParam(SectionObject, TEXT("section_index"), INDEX_NONE, ParamState);
            Request.ExpectedMaterialSlot = GetStringParam(SectionObject, TEXT("expected_material_slot"), ParamState);
            Request.MaterialPath = GetStringParam(SectionObject, TEXT("material_path"), ParamState);

            double MaxDistance = 0.0;
            const TSharedPtr<FJsonValue>* MaxDistanceValue = FindJson(SectionObject,TEXT("max_distance_cm"));
            if (!MaxDistanceValue || !ReadNumber(*MaxDistanceValue, MaxDistance) || !IsFiniteFloatRange(MaxDistance))
            {
                ParamState.AddError(FString::Printf(TEXT("section_configs[%d].max_distance_cm must be a finite JSON number"), ArrayIndex));
            }
            else
            {
                Request.MaxDistanceCm = static_cast<float>(MaxDistance);
            }

            if (Request.SectionIndex < 0)
            {
                ParamState.AddError(FString::Printf(TEXT("section_configs[%d].section_index must be non-negative"), ArrayIndex));
            }
            if (Request.ExpectedMaterialSlot.IsEmpty())
            {
                ParamState.AddError(FString::Printf(TEXT("section_configs[%d].expected_material_slot is required"), ArrayIndex));
            }
            if (Request.MaxDistanceCm <= 0.f || Request.MaxDistanceCm > 100.f)
            {
                ParamState.AddError(FString::Printf(TEXT("section_configs[%d].max_distance_cm must be in (0, 100]"), ArrayIndex));
            }
            if (RequestedSections.Contains(Request.SectionIndex))
            {
                ParamState.AddError(FString::Printf(TEXT("section_configs contains duplicate section_index %d"), Request.SectionIndex));
            }
            RequestedSections.Add(Request.SectionIndex);
            SectionRequests.Add(MoveTemp(Request));
        }
    }

    Work->R.bSaveRequested=bSaveAssets;
    if (!ParamState.IsValid())
    {
        return FLegacy::CreateErrorResponse(ParamState.GetErrorMessage());
    }
    if (SourceSkeletalMeshPath.IsEmpty() || OutputSkeletalMeshPath.IsEmpty())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("source_skeletal_mesh_path and output_skeletal_mesh_path are required"));
    }
    if (!bDryRun && IsPlaySessionActive())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Legacy Chaos Cloth creation is blocked while PIE/SIE is active"));
    }

    if(Dependency(SourceSkeletalMeshPath)||Dependency(OutputSkeletalMeshPath)) { return ErrorResponse(TEXT("MCP-only source/output dependency reference rejected")); }
    OutputSkeletalMeshPath.TrimStartAndEndInline();
    if (!OutputSkeletalMeshPath.StartsWith(TEXT("/Game/_MCP_Temp/")) ||
        !FPackageName::IsValidLongPackageName(OutputSkeletalMeshPath))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("output_skeletal_mesh_path must be a valid new asset package below /Game/_MCP_Temp"));
    }
    const FString OutputAssetName = FPackageName::GetLongPackageAssetName(OutputSkeletalMeshPath);
    const FString OutputAssetDirectory = FPackageName::GetLongPackagePath(OutputSkeletalMeshPath);
    const FString OutputObjectPath = FString::Printf(TEXT("%s.%s"), *OutputSkeletalMeshPath, *OutputAssetName);
    const FString OutputClothPhysicsAssetName = OutputAssetName + TEXT("_ClothPhysicsAsset");
    const FString OutputClothPhysicsAssetPath = FString::Printf(
        TEXT("%s/%s"),
        *OutputAssetDirectory,
        *OutputClothPhysicsAssetName);
    const FString OutputClothPhysicsObjectPath = FString::Printf(
        TEXT("%s.%s"),
        *OutputClothPhysicsAssetPath,
        *OutputClothPhysicsAssetName);
    Work->OutputPath=OutputObjectPath;Work->OutputPhysicsPath=OutputClothPhysicsObjectPath;
    if(FindObject<UObject>(nullptr,*OutputObjectPath)||FindPackage(nullptr,*OutputSkeletalMeshPath)||FPackageName::DoesPackageExist(OutputSkeletalMeshPath)||(bCreateVroidLowerBodyClothPhysicsAsset&&(FindObject<UObject>(nullptr,*OutputClothPhysicsObjectPath)||FindPackage(nullptr,*OutputClothPhysicsAssetPath)||FPackageName::DoesPackageExist(OutputClothPhysicsAssetPath)))) { return ErrorResponse(TEXT("Output object/package already exists in memory or on disk; no overwrite is performed")); }
    SDK(TEXT("output_registry_admission"));
    if (OutputAssetName.IsEmpty() || OutputAssetDirectory.IsEmpty())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("output_skeletal_mesh_path must include an asset name"));
    }
    if (FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(OutputObjectPath)).IsValid())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("output_skeletal_mesh_path already exists; this command never overwrites a Skeletal Mesh"));
    }
    if (bCreateVroidLowerBodyClothPhysicsAsset &&
        FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(
            FSoftObjectPath(OutputClothPhysicsObjectPath)).IsValid())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("The derived VRoid Cloth Physics Asset output already exists; this command never overwrites assets"));
    }

    SDK(TEXT("load_source_skeletal_mesh"));Count(TEXT("source_load_calls"));
    USkeletalMesh* const SourceMesh = LoadObject<USkeletalMesh>(nullptr, *SourceSkeletalMeshPath);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    if (!SourceMesh)
    {
        return FLegacy::CreateErrorResponse(
            FString::Printf(TEXT("Unable to load skeletal mesh '%s'"), *SourceSkeletalMeshPath));
    }
    if(!Live(SourceMesh)||Dependency(SourceMesh->GetClass()->GetPathName())) { return ErrorResponse(TEXT("Source mesh is not a live native runtime-safe target")); }
    Work->Source.Reset(SourceMesh);Work->SourcePhysics.Reset(SourceMesh->GetPhysicsAsset());if(Work->SourcePhysics.Get()!=nullptr&&(!Live(Work->SourcePhysics.Get())||Dependency(Work->SourcePhysics->GetClass()->GetPathName()))) { return ErrorResponse(TEXT("Source Physics Asset is not a live native runtime-safe reference")); }Work->SourceCaptured=true;Work->SourcePath=SourceMesh->GetPathName();Work->SourcePackage=SourceMesh->GetPackage();Work->SourcePackagePath=SourceMesh->GetPackage()->GetName();Work->SourceClass=SourceMesh->GetClass();Work->BeforeSource=Work->SourceProjection();Work->R.Data->SetObjectField(TEXT("source_before"),Work->BeforeSource);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }SDK(TEXT("read_imported_source_model"));
    const FSkeletalMeshModel* const SourceModel = SourceMesh->GetImportedModel();
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    if (!SourceModel || !SourceModel->LODModels.IsValidIndex(0))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("The source Skeletal Mesh has no imported LOD 0 model"));
    }
    constexpr float VroidHipsRadiusToUpperLegLength = 0.38f;
    constexpr float VroidUpperLegRadiusToBoneLength = 0.205f;
    float VroidLeftUpperLegLength = 0.f;
    float VroidRightUpperLegLength = 0.f;
    float VroidHipsRadius = 0.f;
    if (bCreateVroidLowerBodyClothPhysicsAsset)
    {
        if (!SourceMesh->GetPhysicsAsset())
        {
            return FLegacy::CreateErrorResponse(
                TEXT("VRoid lower-body Cloth Physics Asset creation requires a source Physics Asset"));
        }
        const TArray<FName> BodyBones={FName(TEXT("J_Bip_C_Hips")),FName(TEXT("J_Bip_L_UpperLeg")),FName(TEXT("J_Bip_R_UpperLeg"))};
        for(const FName Bone:BodyBones) { if(!SourceMesh->GetPhysicsAsset()->SkeletalBodySetups.ContainsByPredicate([Bone](const auto& B){return B&&B->BoneName==Bone;})) { return ErrorResponse(TEXT("VRoid collision preset requires existing hips and both upper-leg bodies in the source Physics Asset; invalid partial presets are not generated")); } }
        const FReferenceSkeleton& RefSkeleton = SourceMesh->GetRefSkeleton();
        const TPair<FName, FName> RequiredBonePairs[] = {
            {FName(TEXT("J_Bip_L_UpperLeg")), FName(TEXT("J_Bip_L_LowerLeg"))},
            {FName(TEXT("J_Bip_R_UpperLeg")), FName(TEXT("J_Bip_R_LowerLeg"))}};
        if (RefSkeleton.FindBoneIndex(FName(TEXT("J_Bip_C_Hips"))) == INDEX_NONE)
        {
            return FLegacy::CreateErrorResponse(
                TEXT("VRoid lower-body Cloth Physics Asset creation requires J_Bip_C_Hips"));
        }
        for (const TPair<FName, FName>& Pair : RequiredBonePairs)
        {
            const int32 ParentIndex = RefSkeleton.FindBoneIndex(Pair.Key);
            const int32 ChildIndex = RefSkeleton.FindBoneIndex(Pair.Value);
            if (ParentIndex == INDEX_NONE || ChildIndex == INDEX_NONE ||
                RefSkeleton.GetParentIndex(ChildIndex) != ParentIndex)
            {
                return FLegacy::CreateErrorResponse(FString::Printf(
                    TEXT("VRoid lower-body Cloth Physics Asset requires direct bone pair %s -> %s"),
                    *Pair.Key.ToString(),
                    *Pair.Value.ToString()));
            }
        }
        const int32 LeftLowerLegIndex = RefSkeleton.FindBoneIndex(FName(TEXT("J_Bip_L_LowerLeg")));
        const int32 RightLowerLegIndex = RefSkeleton.FindBoneIndex(FName(TEXT("J_Bip_R_LowerLeg")));
        VroidLeftUpperLegLength = RefSkeleton.GetRefBonePose()[LeftLowerLegIndex].GetTranslation().Size();
        VroidRightUpperLegLength = RefSkeleton.GetRefBonePose()[RightLowerLegIndex].GetTranslation().Size();
        if (VroidLeftUpperLegLength <= KINDA_SMALL_NUMBER ||
            VroidRightUpperLegLength <= KINDA_SMALL_NUMBER)
        {
            return FLegacy::CreateErrorResponse(
                TEXT("VRoid lower-body Cloth Physics Asset requires non-zero upper-leg reference bone lengths"));
        }
        VroidHipsRadius =
            ((VroidLeftUpperLegLength + VroidRightUpperLegLength) * 0.5f) *
            VroidHipsRadiusToUpperLegLength;
    }

    const TArray<FSkeletalMaterial> SourceMaterials = SourceMesh->GetMaterials();
    TMap<int32, UMaterialInterface*> RenderMaterialsBySlot;
    for (FSectionRequest& Request : SectionRequests)
    {
        SDK(TEXT("read_section_preflight"));const FSkeletalMeshModel* CurrentSourceModel=SourceMesh->GetImportedModel();if(!Guard()) { return ErrorResponse(Work->R.Error); }
        if(!CurrentSourceModel||!CurrentSourceModel->LODModels.IsValidIndex(0)) { return ErrorResponse(TEXT("Source imported LOD0 became unavailable")); }
        const FSkeletalMeshLODModel& SourceLod=CurrentSourceModel->LODModels[0];
        if (!SourceLod.Sections.IsValidIndex(Request.SectionIndex))
        {
            return FLegacy::CreateErrorResponse(
                FString::Printf(TEXT("section_index %d is not a valid imported LOD 0 section"), Request.SectionIndex));
        }
        const FSkelMeshSection Section = SourceLod.Sections[Request.SectionIndex];
        if (!SourceMaterials.IsValidIndex(Section.MaterialIndex))
        {
            return FLegacy::CreateErrorResponse(
                FString::Printf(TEXT("section_index %d has no valid material slot"), Request.SectionIndex));
        }
        Request.ActualMaterialSlot = SourceMaterials[Section.MaterialIndex].MaterialSlotName.ToString();
        Request.ExpectedOriginalDataSectionIndex = Section.OriginalDataSectionIndex;
        if (Request.ActualMaterialSlot != Request.ExpectedMaterialSlot)
        {
            return FLegacy::CreateErrorResponse(
                FString::Printf(
                    TEXT("section %d material-slot mismatch: expected '%s', found '%s'"),
                    Request.SectionIndex,
                    *Request.ExpectedMaterialSlot,
                    *Request.ActualMaterialSlot));
        }
        if(Dependency(Request.MaterialPath)) { return ErrorResponse(TEXT("MCP-only clothing material dependency reference rejected")); }SDK(TEXT("resolve_render_material"));if(!Request.MaterialPath.IsEmpty()) { Count(TEXT("material_load_calls")); }
        Request.RenderMaterial = Request.MaterialPath.IsEmpty()
            ? SourceMaterials[Section.MaterialIndex].MaterialInterface.Get()
            : LoadObject<UMaterialInterface>(nullptr, *Request.MaterialPath);
        Request.MaterialSource = Request.MaterialPath.IsEmpty()
            ? TEXT("source_material_slot")
            : TEXT("section_config.material_path");
        if (!Request.RenderMaterial)
        {
            return FLegacy::CreateErrorResponse(FString::Printf(
                TEXT("section %d material slot '%s' has no render material; provide section_configs.material_path"),
                Request.SectionIndex,
                *Request.ActualMaterialSlot));
        }
        if(!Live(Request.RenderMaterial)||Dependency(Request.RenderMaterial->GetClass()->GetPathName())) { return ErrorResponse(TEXT("Render material is not a live native runtime-safe target")); }
        Work->Materials.Emplace(Request.RenderMaterial);if(!Guard()) { return ErrorResponse(Work->R.Error); }SDK(TEXT("inspect_existing_material_usage"));
        const bool bUsedWithSkeletalMesh =
            HasMaterialUsageWithoutMutation(Request.RenderMaterial, MATUSAGE_SkeletalMesh);
        const bool bUsedWithClothing =
            HasMaterialUsageWithoutMutation(Request.RenderMaterial, MATUSAGE_Clothing);
        if(!Guard()) { return ErrorResponse(Work->R.Error); }
        if (!bUsedWithSkeletalMesh || !bUsedWithClothing)
        {
            return FLegacy::CreateErrorResponse(FString::Printf(
                TEXT("section %d render material '%s' must already be compiled with Used with Skeletal Mesh and Used with Clothing"),
                Request.SectionIndex,
                *Request.RenderMaterial->GetPathName()));
        }
        if (UMaterialInterface** ExistingMaterial = RenderMaterialsBySlot.Find(Section.MaterialIndex))
        {
            if (*ExistingMaterial != Request.RenderMaterial)
            {
                return FLegacy::CreateErrorResponse(FString::Printf(
                    TEXT("section_configs assigns conflicting render materials to material slot index %d"),
                    Section.MaterialIndex));
            }
        }
        else
        {
            RenderMaterialsBySlot.Add(Section.MaterialIndex, Request.RenderMaterial);
        }
        if (Section.HasClothingData() || Section.NumVertices <= 0 || Section.NumTriangles <= 0 ||
            Section.SoftVertices.Num() != Section.NumVertices)
        {
            return FLegacy::CreateErrorResponse(
                FString::Printf(TEXT("section_index %d is already bound or has no usable imported geometry"), Request.SectionIndex));
        }
        Request.VertexCount = Section.NumVertices;
        Request.TriangleCount = Section.NumTriangles;
        float MinZ = TNumericLimits<float>::Max();
        float MaxZ = TNumericLimits<float>::Lowest();
        for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
        {
            const float GuideWeight = static_cast<float>(Vertex.Color.R) / 255.f;
            Request.PinnedGuideVertexCount += GuideWeight <= KINDA_SMALL_NUMBER ? 1 : 0;
            Request.DynamicGuideVertexCount += GuideWeight >= (1.f - KINDA_SMALL_NUMBER) ? 1 : 0;
            MinZ = FMath::Min(MinZ, Vertex.Position.Z);
            MaxZ = FMath::Max(MaxZ, Vertex.Position.Z);
        }
        if (Request.PinnedGuideVertexCount == 0 || Request.DynamicGuideVertexCount == 0)
        {
            const float VerticalSpan = MaxZ - MinZ;
            if (VerticalSpan <= KINDA_SMALL_NUMBER)
            {
                return FLegacy::CreateErrorResponse(
                    FString::Printf(
                        TEXT("section_index %d has no usable Cloth_PinGuide colors or vertical range for a safe fallback"),
                        Request.SectionIndex));
            }
            Request.bUseVerticalPinFallback = true;
            Request.VerticalPinFallbackThreshold = MaxZ - (VerticalSpan * 0.18f);
            Request.PinnedGuideVertexCount = 0;
            Request.DynamicGuideVertexCount = 0;
            for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
            {
                const bool bPinned = Vertex.Position.Z >= Request.VerticalPinFallbackThreshold;
                Request.PinnedGuideVertexCount += bPinned ? 1 : 0;
                Request.DynamicGuideVertexCount += bPinned ? 0 : 1;
            }
        }
    }

    TSharedPtr<FJsonObject> Before = MakeShared<FJsonObject>();
    Before->SetStringField(TEXT("source_skeletal_mesh_path"), SourceMesh->GetPathName());
    Before->SetStringField(TEXT("output_skeletal_mesh_path"), OutputSkeletalMeshPath);
    Before->SetNumberField(TEXT("source_clothing_asset_count"), SourceMesh->GetMeshClothingAssets().Num());

    TArray<TSharedPtr<FJsonValue>> SectionPlan;
    for (const FSectionRequest& Request : SectionRequests)
    {
        TSharedPtr<FJsonObject> SectionResult = MakeShared<FJsonObject>();
        SectionResult->SetNumberField(TEXT("section_index"), Request.SectionIndex);
        SectionResult->SetStringField(TEXT("material_slot"), Request.ActualMaterialSlot);
        SectionResult->SetStringField(TEXT("material_path"), Request.RenderMaterial->GetPathName());
        SectionResult->SetStringField(TEXT("material_source"), Request.MaterialSource);
        SectionResult->SetBoolField(TEXT("used_with_skeletal_mesh"), true);
        SectionResult->SetBoolField(TEXT("used_with_clothing"), true);
        SectionResult->SetNumberField(TEXT("max_distance_cm"), Request.MaxDistanceCm);
        SectionResult->SetNumberField(TEXT("vertex_count"), Request.VertexCount);
        SectionResult->SetNumberField(TEXT("triangle_count"), Request.TriangleCount);
        SectionResult->SetNumberField(TEXT("pinned_guide_vertex_count"), Request.PinnedGuideVertexCount);
        SectionResult->SetNumberField(TEXT("dynamic_guide_vertex_count"), Request.DynamicGuideVertexCount);
        SectionResult->SetStringField(
            TEXT("pin_source"),
            Request.bUseVerticalPinFallback
                ? TEXT("FBX vertex colors unavailable; top 18 percent of local Z range pinned")
                : TEXT("Cloth_PinGuide vertex-color red channel"));
        SectionPlan.Add(MakeShared<FJsonValueObject>(SectionResult));
    }
    TSharedPtr<FJsonObject> Plan = MakeShared<FJsonObject>();
    Plan->SetStringField(TEXT("authoring_path"), TEXT("UE 5.8 Skeletal Mesh Clothing / native Chaos legacy simulation"));
    Plan->SetStringField(TEXT("source_skeletal_mesh_path"), SourceMesh->GetPathName());
    Plan->SetStringField(TEXT("output_skeletal_mesh_path"), OutputSkeletalMeshPath);
    Plan->SetBoolField(TEXT("use_source_physics_asset"), bUseSourcePhysicsAsset);
    Plan->SetBoolField(
        TEXT("create_vroid_lower_body_cloth_physics_asset"),
        bCreateVroidLowerBodyClothPhysicsAsset);
    Plan->SetStringField(
        TEXT("cloth_collision_source"),
        bCreateVroidLowerBodyClothPhysicsAsset
            ? TEXT("generated VRoid hips and upper-leg Cloth Physics Asset")
            : bUseSourcePhysicsAsset
            ? TEXT("source Skeletal Mesh Physics Asset")
            : TEXT("none; Character Capsule still handles Pawn movement collision"));
    Plan->SetStringField(
        TEXT("generated_cloth_physics_asset_path"),
        bCreateVroidLowerBodyClothPhysicsAsset ? OutputClothPhysicsAssetPath : FString());
    if (bCreateVroidLowerBodyClothPhysicsAsset)
    {
        Plan->SetNumberField(TEXT("vroid_left_upper_leg_length_cm"), VroidLeftUpperLegLength);
        Plan->SetNumberField(TEXT("vroid_right_upper_leg_length_cm"), VroidRightUpperLegLength);
        Plan->SetNumberField(TEXT("vroid_hips_radius_cm"), VroidHipsRadius);
        Plan->SetNumberField(
            TEXT("vroid_left_upper_leg_radius_cm"),
            VroidLeftUpperLegLength * VroidUpperLegRadiusToBoneLength);
        Plan->SetNumberField(
            TEXT("vroid_right_upper_leg_radius_cm"),
            VroidRightUpperLegLength * VroidUpperLegRadiusToBoneLength);
    }
    Plan->SetArrayField(TEXT("sections"), SectionPlan);
    Plan->SetStringField(TEXT("pin_guide_source"), TEXT("Cloth_PinGuide red channel; if FBX colors are unavailable, top 18 percent of local Z is pinned"));
    Plan->SetStringField(
        TEXT("simulation_preset"),
        TEXT("light controlled fabric: density 0.10, edge/area stiffness 0.45, bend stiffness 0.05, tether stiffness 0.35, anim drive 0.15, 2 substeps"));

    if (bDryRun)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetObjectField(TEXT("plan"), Plan);
        FLegacy::AddMutationResultContract(
            Result,
            true,
            false,
            false,
            Before,
            Plan,
            MakeShared<FJsonObject>(),
            true,
            TEXT("source_sections_and_Cloth_PinGuide_preflight"),
            {TEXT("new_temp_output"), TEXT("material_slots"), TEXT("black_and_white_pin_guide")});
        return Result;
    }

    if(!Json(Work->BeforeSource).Equals(Json(Work->SourceProjection()),ESearchCase::CaseSensitive)||!Guard()) { return ErrorResponse(TEXT("Source mesh changed during preflight; no output creation was started")); }
    SDK(TEXT("load_clothing_factory_provider"));
    if(!FModuleManager::Get().LoadModulePtr<IModuleInterface>(TEXT("ClothingSystemEditor"))) { return ErrorResponse(TEXT("The native ClothingSystemEditor factory module could not be loaded")); }
    FClothingSystemEditorInterfaceModule& ClothingInterface =
        FModuleManager::LoadModuleChecked<FClothingSystemEditorInterfaceModule>(TEXT("ClothingSystemEditorInterface"));
    UClothingAssetFactoryBase* const ClothingFactory = ClothingInterface.GetClothingAssetFactory();
    if (!ClothingFactory)
    {
        return FLegacy::CreateErrorResponse(
            TEXT("The UE ClothingSystemEditor factory provider is unavailable"));
    }

    Work->Factory.Reset(ClothingFactory);if(!Guard()) { return ErrorResponse(Work->R.Error); }Work->Prefix->SetStringField(TEXT("factory_class"),ClothingFactory->GetClass()->GetPathName());
    Work->bOuterTransaction=GEditor->IsTransactionActive();Work->Write(TEXT("begin_owned_creation_transaction"));
    Work->Transaction=MakeUnique<FScopedTransaction>(NSLOCTEXT(
        "UnrealMCP",
        "CreateLegacyChaosClothingFromSkeletalSections",
        "Create Legacy Chaos Clothing From Skeletal Mesh Sections"));
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    if(!Guard()||!AdmitFreshOutput(OutputSkeletalMeshPath,OutputObjectPath)) { return ErrorResponse(Work->R.Error); }Work->R.Data->SetBoolField(TEXT("SDK_indirect_save_possible"),true);Work->R.Data->SetBoolField(TEXT("SDK_indirect_save_success_known"),false);SDK(TEXT("duplicate_output_mesh"));Count(TEXT("duplicate_calls"));
    USkeletalMesh* const OutputMesh = Cast<USkeletalMesh>(
        AssetTools.DuplicateAsset(OutputAssetName, OutputAssetDirectory, SourceMesh));Count(TEXT("duplicate_returns"));
    if (!OutputMesh)
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Failed to duplicate the source Skeletal Mesh for cloth authoring"));
    }
    if(!Work->Own(OutputMesh,OutputObjectPath)) { return ErrorResponse(Work->R.Error); }
    if(OutputMesh->GetPhysicsAsset()!=Work->SourcePhysics.Get()) { return ErrorResponse(TEXT("Duplicated mesh Physics Asset reference differs from the captured source selection")); }
    Count(TEXT("explicit_modify_calls"));OutputMesh->Modify();if(!Guard()) { return ErrorResponse(Work->R.Error); }

    TArray<FSkeletalMaterial> OutputMaterials = OutputMesh->GetMaterials();
    for (const TPair<int32, UMaterialInterface*>& Pair : RenderMaterialsBySlot)
    {
        if (!OutputMaterials.IsValidIndex(Pair.Key) || !Pair.Value)
        {
            const FOwnedAssetCleanupResult Cleanup =
                DeleteOwnedAssetsWithoutConfirmation({OutputMesh});
            return FLegacy::CreateErrorResponse(FString::Printf(
                TEXT("Failed to assign validated clothing render material; rollback_succeeded=%s"),
                Cleanup.bSucceeded ? TEXT("true") : TEXT("false")));
        }
        OutputMaterials[Pair.Key].MaterialInterface = Pair.Value;
    }
    SDK(TEXT("assign_owned_output_materials"));OutputMesh->SetMaterials(OutputMaterials);if(!Guard()) { return ErrorResponse(Work->R.Error); }

    UPhysicsAsset* GeneratedClothPhysicsAsset = nullptr;
    auto RollbackOwnedOutputs = [&OutputMesh, &GeneratedClothPhysicsAsset]()
    {
        TArray<UObject*> OwnedAssets;
        if (GeneratedClothPhysicsAsset)
        {
            OwnedAssets.Add(GeneratedClothPhysicsAsset);
        }
        OwnedAssets.Add(OutputMesh);
        return DeleteOwnedAssetsWithoutConfirmation(OwnedAssets);
    };
    if (bCreateVroidLowerBodyClothPhysicsAsset)
    {
        if(!Guard()||!AdmitFreshOutput(OutputClothPhysicsAssetPath,OutputClothPhysicsObjectPath)) { return ErrorResponse(Work->R.Error); }SDK(TEXT("duplicate_owned_cloth_physics"));Count(TEXT("duplicate_calls"));
        GeneratedClothPhysicsAsset = Cast<UPhysicsAsset>(AssetTools.DuplicateAsset(
            OutputClothPhysicsAssetName,
            OutputAssetDirectory,
            Work->SourcePhysics.Get()));Count(TEXT("duplicate_returns"));
        if (!GeneratedClothPhysicsAsset)
        {
            const FOwnedAssetCleanupResult Cleanup = RollbackOwnedOutputs();
            return FLegacy::CreateErrorResponse(FString::Printf(
                TEXT("Failed to duplicate the source Physics Asset for VRoid Cloth collision; rollback_succeeded=%s"),
                Cleanup.bSucceeded ? TEXT("true") : TEXT("false")));
        }
        if(!Work->Own(GeneratedClothPhysicsAsset,OutputClothPhysicsObjectPath)) { return ErrorResponse(Work->R.Error); }
        Count(TEXT("explicit_modify_calls"));GeneratedClothPhysicsAsset->Modify();if(!Guard()) { return ErrorResponse(Work->R.Error); }
        const TSet<FName> RequiredBodyBones = {
            FName(TEXT("J_Bip_C_Hips")),
            FName(TEXT("J_Bip_L_UpperLeg")),
            FName(TEXT("J_Bip_R_UpperLeg"))};
        GeneratedClothPhysicsAsset->SkeletalBodySetups.RemoveAll(
            [&RequiredBodyBones](const TObjectPtr<USkeletalBodySetup>& BodySetup)
            {
                return !BodySetup || !RequiredBodyBones.Contains(BodySetup->BoneName);
            });
        GeneratedClothPhysicsAsset->ConstraintSetup.Reset();

        const FReferenceSkeleton& RefSkeleton = OutputMesh->GetRefSkeleton();
        for (USkeletalBodySetup* BodySetup : GeneratedClothPhysicsAsset->SkeletalBodySetups)
        {
            if(!Live(BodySetup)||!BodySetup->IsIn(GeneratedClothPhysicsAsset)||(Work->SourcePhysics.IsValid()&&Work->SourcePhysics->SkeletalBodySetups.Contains(BodySetup))||!Guard()) { return ErrorResponse(TEXT("Created physics body is no longer live")); }
            if(!Work->OwnChild(BodySetup,GeneratedClothPhysicsAsset)) { return ErrorResponse(Work->R.Error); }
            Count(TEXT("explicit_modify_calls"));BodySetup->Modify();if(!Guard()) { return ErrorResponse(Work->R.Error); }
            BodySetup->AggGeom = FKAggregateGeom();
            if (BodySetup->BoneName == FName(TEXT("J_Bip_C_Hips")))
            {
                FKSphereElem HipsSphere(VroidHipsRadius);
                HipsSphere.Center = FVector::ZeroVector;
                BodySetup->AggGeom.SphereElems.Add(HipsSphere);
            }
            else
            {
                const FName LowerLegBone = BodySetup->BoneName == FName(TEXT("J_Bip_L_UpperLeg"))
                    ? FName(TEXT("J_Bip_L_LowerLeg"))
                    : FName(TEXT("J_Bip_R_LowerLeg"));
                const int32 ChildIndex = RefSkeleton.FindBoneIndex(LowerLegBone);
                if(ChildIndex==INDEX_NONE) { return ErrorResponse(TEXT("Duplicated physics mesh reference bone became unavailable")); }
                const FVector ChildTranslation = RefSkeleton.GetRefBonePose()[ChildIndex].GetTranslation();
                const float ChildLength = ChildTranslation.Size();
                const float Radius = ChildLength * VroidUpperLegRadiusToBoneLength;
                FKSphylElem UpperLegCapsule(
                    Radius,
                    FMath::Max(ChildLength - (2.f * Radius), ChildLength * 0.05f));
                UpperLegCapsule.Center = ChildTranslation * 0.5f;
                UpperLegCapsule.Rotation = FQuat::FindBetweenNormals(
                    FVector::UpVector,
                    ChildTranslation.GetSafeNormal()).Rotator();
                BodySetup->AggGeom.SphylElems.Add(UpperLegCapsule);
            }
            SDK(TEXT("cook_owned_collision_body"));BodySetup->InvalidatePhysicsData();if(!Guard()) { return ErrorResponse(Work->R.Error); }Count(TEXT("physics_cook_calls"));
            BodySetup->CreatePhysicsMeshes();if(!Guard()) { return ErrorResponse(Work->R.Error); }
        }
        GeneratedClothPhysicsAsset->UpdateBodySetupIndexMap();
        GeneratedClothPhysicsAsset->UpdateBoundsBodiesArray();
        GeneratedClothPhysicsAsset->RefreshPhysicsAssetChange();if(!Guard()) { return ErrorResponse(Work->R.Error); }
        GeneratedClothPhysicsAsset->MarkPackageDirty();if(!Guard()) { return ErrorResponse(Work->R.Error); }
        TArray<TSharedPtr<FJsonValue>> ShapeRows;bool PresetValid=GeneratedClothPhysicsAsset->SkeletalBodySetups.Num()==3&&GeneratedClothPhysicsAsset->ConstraintSetup.IsEmpty();TSet<FName> SeenBones;
        for(const USkeletalBodySetup* Body:GeneratedClothPhysicsAsset->SkeletalBodySetups)
        {
            auto Row=MakeShared<FJsonObject>();const bool BodyValid=Live(Body)&&Body->IsIn(GeneratedClothPhysicsAsset);Row->SetBoolField(TEXT("owned_body_live"),BodyValid);bool ShapeValid=BodyValid;
            if(BodyValid)
            {
                Row->SetStringField(TEXT("bone"),Body->BoneName.ToString());Row->SetNumberField(TEXT("shape_count"),Body->AggGeom.GetElementCount());ShapeValid=RequiredBodyBones.Contains(Body->BoneName)&&!SeenBones.Contains(Body->BoneName)&&Body->AggGeom.GetElementCount()==1;SeenBones.Add(Body->BoneName);
                if(Body->BoneName==FName(TEXT("J_Bip_C_Hips")))
                {
                    ShapeValid&=Body->AggGeom.SphereElems.Num()==1;
                    if(Body->AggGeom.SphereElems.Num()==1) { const auto& Shape=Body->AggGeom.SphereElems[0];ShapeValid&=FMath::IsNearlyEqual(Shape.Radius,VroidHipsRadius,KINDA_SMALL_NUMBER)&&Shape.Center.Equals(FVector::ZeroVector,KINDA_SMALL_NUMBER);Row->SetNumberField(TEXT("radius_cm"),Shape.Radius); }
                }
                else
                {
                    ShapeValid&=Body->AggGeom.SphylElems.Num()==1;const FName Lower=Body->BoneName==FName(TEXT("J_Bip_L_UpperLeg"))?FName(TEXT("J_Bip_L_LowerLeg")):FName(TEXT("J_Bip_R_LowerLeg"));const int32 Child=RefSkeleton.FindBoneIndex(Lower);ShapeValid&=Child!=INDEX_NONE;
                    if(Child!=INDEX_NONE&&Body->AggGeom.SphylElems.Num()==1) { const auto& Shape=Body->AggGeom.SphylElems[0];const FVector Translation=RefSkeleton.GetRefBonePose()[Child].GetTranslation();const float Length=Translation.Size(),Radius=Length*VroidUpperLegRadiusToBoneLength;ShapeValid&=FMath::IsNearlyEqual(Shape.Radius,Radius,KINDA_SMALL_NUMBER)&&FMath::IsNearlyEqual(Shape.Length,FMath::Max(Length-2.f*Radius,Length*0.05f),KINDA_SMALL_NUMBER)&&Shape.Center.Equals(Translation*0.5f,KINDA_SMALL_NUMBER)&&Shape.Rotation.Quaternion().Equals(FQuat::FindBetweenNormals(FVector::UpVector,Translation.GetSafeNormal()),KINDA_SMALL_NUMBER);Row->SetNumberField(TEXT("radius_cm"),Shape.Radius);Row->SetNumberField(TEXT("length_cm"),Shape.Length); }
                }
            }
            Row->SetBoolField(TEXT("shape_matches_preset"),ShapeValid);PresetValid&=ShapeValid;ShapeRows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Work->R.Data->SetArrayField(TEXT("generated_collision_readback"),ShapeRows);Work->Checks->SetBoolField(TEXT("generated_collision_preset_matches"),PresetValid);
        if(!PresetValid) { return ErrorResponse(TEXT("Generated VRoid collision bodies do not match the admitted hips and upper-leg preset")); }
    }

    if(!Guard()) { return ErrorResponse(Work->R.Error); }SDK(TEXT("new_shared_simulation_config"));Count(TEXT("explicit_new_config_calls"));
    UChaosClothSharedSimConfig* const SharedConfig = NewObject<UChaosClothSharedSimConfig>(
        OutputMesh,
        TEXT("ChaosSharedSimulationConfig"),
        RF_Transactional);
    if (!SharedConfig)
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Failed to create the shared Chaos Cloth simulation config"));
    }

    if(!Work->OwnChild(SharedConfig,OutputMesh)) { return ErrorResponse(Work->R.Error); }
    TArray<UClothingAssetCommon*> CreatedClothingAssets;
    CreatedClothingAssets.Reserve(SectionRequests.Num());
    bool bCreationSucceeded = true;
    FString CreationError;
    {
        // Match the Skeletal Mesh Editor's apply-clothing lifecycle. The outer scope defers
        // PostEditChange until both BindToSkeletalMesh and UserSectionsData are synchronized;
        // without it the early rebuild discards ClothMappingDataLODs from the render sections.
        FObservedPostEditScope ScopedPostEditChange(OutputMesh);
        if(!Guard()) { return ErrorResponse(Work->R.Error); }

        SharedConfig->IterationCount = 6;
        SharedConfig->MaxIterationCount = 12;
        SharedConfig->SubdivisionCount = 2;

        for (const FSectionRequest& Request : SectionRequests)
        {
            FSkeletalMeshClothBuildParams BuildParams;
            BuildParams.AssetName = FString::Printf(TEXT("CA_%s"), *Request.ActualMaterialSlot);
            BuildParams.LodIndex = 0;
            BuildParams.SourceSection = Request.SectionIndex;
            BuildParams.bRemoveFromMesh = false;
            BuildParams.PhysicsAsset = bCreateVroidLowerBodyClothPhysicsAsset
                ? GeneratedClothPhysicsAsset
                : bUseSourcePhysicsAsset
                    ? Work->SourcePhysics.Get()
                    : nullptr;

            if(!Guard()) { return ErrorResponse(Work->R.Error); }SDK(TEXT("factory_create_clothing"));Count(TEXT("factory_calls"));
            UClothingAssetBase* const ReturnedClothing=ClothingFactory->CreateFromSkeletalMesh(OutputMesh,BuildParams);Count(TEXT("factory_returns"));
            auto FactoryRow=MakeShared<FJsonObject>();FactoryRow->SetNumberField(TEXT("section_index"),Request.SectionIndex);FactoryRow->SetBoolField(TEXT("returned_object_live"),Live(ReturnedClothing));
            if(Live(ReturnedClothing)) { FactoryRow->SetStringField(TEXT("returned_path"),ReturnedClothing->GetPathName());FactoryRow->SetStringField(TEXT("returned_class"),ReturnedClothing->GetClass()->GetPathName());FactoryRow->SetBoolField(TEXT("returned_within_owned_mesh"),ReturnedClothing->IsIn(OutputMesh)); }
            Work->Prefix->SetObjectField(TEXT("latest_factory_return"),FactoryRow);UClothingAssetCommon* const ClothingAsset=Cast<UClothingAssetCommon>(ReturnedClothing);
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
            if (!ClothingAsset)
            {
                bCreationSucceeded = false;
                CreationError = FString::Printf(TEXT("Failed to create Clothing Asset from section %d"), Request.SectionIndex);
                break;
            }

            if(!Live(ClothingAsset)||!ClothingAsset->IsIn(OutputMesh)||Dependency(ClothingAsset->GetClass()->GetPathName())) { return ErrorResponse(TEXT("Factory returned an unsupported runtime-dependent clothing asset")); }
            if(!Work->OwnChild(ClothingAsset,OutputMesh)) { return ErrorResponse(Work->R.Error); }
            SDK(TEXT("register_created_clothing"));OutputMesh->AddClothingAsset(ClothingAsset);if(!Guard()) { return ErrorResponse(Work->R.Error); }
            Count(TEXT("explicit_modify_calls"));ClothingAsset->Modify();if(!Guard()) { return ErrorResponse(Work->R.Error); }
            SDK(TEXT("new_section_Chaos_config"));Count(TEXT("explicit_new_config_calls"));
            UChaosClothConfig* const ChaosConfig = NewObject<UChaosClothConfig>(
                ClothingAsset,
                UChaosClothConfig::StaticClass(),
                NAME_None,
                RF_Transactional);
            if (!ChaosConfig)
            {
                bCreationSucceeded = false;
                CreationError = FString::Printf(TEXT("Failed to create a Chaos config for section %d"), Request.SectionIndex);
                break;
            }
            if(!Work->OwnChild(ChaosConfig,ClothingAsset)) { return ErrorResponse(Work->R.Error); }
            ChaosConfig->Density = 0.10f;
            ChaosConfig->EdgeStiffnessWeighted.Low = 0.45f;
            ChaosConfig->EdgeStiffnessWeighted.High = 0.45f;
            ChaosConfig->BendingStiffnessWeighted.Low = 0.05f;
            ChaosConfig->BendingStiffnessWeighted.High = 0.05f;
            ChaosConfig->AreaStiffnessWeighted.Low = 0.45f;
            ChaosConfig->AreaStiffnessWeighted.High = 0.45f;
            ChaosConfig->TetherStiffness.Low = 0.35f;
            ChaosConfig->TetherStiffness.High = 0.35f;
            ChaosConfig->CollisionThickness = 0.02f;
            ChaosConfig->FrictionCoefficient = 0.10f;
            ChaosConfig->DampingCoefficient = 0.005f;
            ChaosConfig->AnimDriveStiffness.Low = 0.15f;
            ChaosConfig->AnimDriveStiffness.High = 0.15f;
            ChaosConfig->AnimDriveDamping.Low = 0.f;
            ChaosConfig->AnimDriveDamping.High = 0.f;
            ChaosConfig->LinearVelocityScale = FVector(1.f, 1.f, 1.f);
            ChaosConfig->AngularVelocityScale = 1.f;
            ChaosConfig->FictitiousAngularScale = 1.f;
            ChaosConfig->GravityScale = 1.0f;
            ClothingAsset->ClothConfigs.Add(ChaosConfig->GetClass()->GetFName(), ChaosConfig);
            ClothingAsset->ClothConfigs.Add(SharedConfig->GetClass()->GetFName(), SharedConfig);

            if (!ClothingAsset->LodData.IsValidIndex(0))
            {
                bCreationSucceeded = false;
                CreationError = FString::Printf(TEXT("Section %d produced no clothing LOD"), Request.SectionIndex);
                break;
            }
            FClothLODDataCommon& ClothLod = ClothingAsset->LodData[0];
            FPointWeightMap* const MaxDistanceMap = ClothLod.PointWeightMaps.FindByPredicate(
                [](const FPointWeightMap& Map)
                {
                    return Map.CurrentTarget == static_cast<uint8>(EWeightMapTargetCommon::MaxDistance);
                });
            if (!MaxDistanceMap || MaxDistanceMap->Values.Num() != ClothLod.PhysicalMeshData.VertexColors.Num() || MaxDistanceMap->Values.Num()!=ClothLod.PhysicalMeshData.Vertices.Num())
            {
                bCreationSucceeded = false;
                CreationError = FString::Printf(TEXT("Section %d has no valid MaxDistance paint map"), Request.SectionIndex);
                break;
            }
            for (int32 VertexIndex = 0; VertexIndex < MaxDistanceMap->Values.Num(); ++VertexIndex)
            {
                const bool bPinned = Request.bUseVerticalPinFallback
                    ? ClothLod.PhysicalMeshData.Vertices[VertexIndex].Z >= Request.VerticalPinFallbackThreshold
                    : static_cast<float>(ClothLod.PhysicalMeshData.VertexColors[VertexIndex].R) / 255.f <= KINDA_SMALL_NUMBER;
                MaxDistanceMap->Values[VertexIndex] = bPinned ? 0.f : Request.MaxDistanceCm;
            }
            SDK(TEXT("apply_MaxDistance_masks"));Count(TEXT("parameter_mask_calls"));ClothingAsset->ApplyParameterMasks();if(!Guard()) { return ErrorResponse(Work->R.Error); }
            SDK(TEXT("bind_created_clothing"));Count(TEXT("bind_calls"));const bool Bound=ClothingAsset->BindToSkeletalMesh(OutputMesh,0,Request.SectionIndex,0);Count(TEXT("bind_returns"));if(!Guard()) { return ErrorResponse(Work->R.Error); }
            if (!Bound)
            {
                bCreationSucceeded = false;
                CreationError = FString::Printf(TEXT("Failed to bind the Chaos Cloth asset to section %d"), Request.SectionIndex);
                break;
            }
            SDK(TEXT("relookup_bound_source_section"));FSkeletalMeshModel* const BoundMeshModel = OutputMesh->GetImportedModel();if(!Guard()) { return ErrorResponse(Work->R.Error); }
            if (!BoundMeshModel || !BoundMeshModel->LODModels.IsValidIndex(0) ||
                !BoundMeshModel->LODModels[0].Sections.IsValidIndex(Request.SectionIndex))
            {
                bCreationSucceeded = false;
                CreationError = FString::Printf(TEXT("Bound section %d could not be persisted to the Skeletal Mesh source model"), Request.SectionIndex);
                break;
            }
            FSkeletalMeshLODModel& BoundLod = BoundMeshModel->LODModels[0];
            FSkelMeshSection& BoundSection = BoundLod.Sections[Request.SectionIndex];
            const int32 BoundAssetIndex = OutputMesh->GetClothingAssetIndex(ClothingAsset);
            if (BoundAssetIndex == INDEX_NONE)
            {
                bCreationSucceeded = false;
                CreationError = FString::Printf(TEXT("Bound Clothing Asset for section %d is not registered on the Skeletal Mesh"), Request.SectionIndex);
                break;
            }
            FSkelMeshSourceSectionUserData& SourceSectionData =
                BoundLod.UserSectionsData.FindOrAdd(BoundSection.OriginalDataSectionIndex);
            SourceSectionData.CorrespondClothAssetIndex = static_cast<int16>(BoundAssetIndex);
            SourceSectionData.ClothingData.AssetGuid = ClothingAsset->GetAssetGuid();
            SourceSectionData.ClothingData.AssetLodIndex = 0;
            SDK(TEXT("apply_fixed_vertex_masks"));Count(TEXT("parameter_mask_calls"));ClothingAsset->ApplyParameterMasks(true);if(!Guard()) { return ErrorResponse(Work->R.Error); }
            Work->Prefix->SetNumberField(TEXT("completed_section_write_prefix"),CreatedClothingAssets.Num()+1);
            CreatedClothingAssets.Add(ClothingAsset);
        }
    }

    if(!Guard()) { return ErrorResponse(Work->R.Error); }SDK(TEXT("independent_clothing_readback"));
    int32 VerifiedBindingCount = 0;
    int32 VerifiedRenderBindingCount = 0;
    int32 VerifiedPinnedVertexCount = 0;
    int32 VerifiedDynamicVertexCount = 0;
    TArray<TSharedPtr<FJsonValue>> CreatedSectionResults;
    if (bCreationSucceeded && CreatedClothingAssets.Num() == SectionRequests.Num())
    {
        for (int32 RequestIndex = 0; RequestIndex < SectionRequests.Num(); ++RequestIndex)
        {
            const FSectionRequest& Request = SectionRequests[RequestIndex];
            UClothingAssetCommon* const ClothingAsset = CreatedClothingAssets[RequestIndex];
            const FPointWeightMap* const MaxDistanceMap = ClothingAsset->LodData.IsValidIndex(0)
                ? ClothingAsset->LodData[0].PhysicalMeshData.FindWeightMap(EWeightMapTargetCommon::MaxDistance)
                : nullptr;
            int32 PinnedCount = 0;
            int32 DynamicCount = 0;
            if (MaxDistanceMap)
            {
                for (const float Value : MaxDistanceMap->Values)
                {
                    PinnedCount += Value <= KINDA_SMALL_NUMBER ? 1 : 0;
                    DynamicCount += Value > KINDA_SMALL_NUMBER ? 1 : 0;
                }
            }
            const int32 ObservedMapCount=MaxDistanceMap?MaxDistanceMap->Values.Num():0;bool AllMapValuesValid=MaxDistanceMap&&ObservedMapCount==ClothingAsset->LodData[0].PhysicalMeshData.Vertices.Num();
            if(MaxDistanceMap) { for(float V:MaxDistanceMap->Values) { AllMapValuesValid&=FMath::IsFinite(V)&&(FMath::Abs(V)<=KINDA_SMALL_NUMBER||FMath::IsNearlyEqual(V,Request.MaxDistanceCm,KINDA_SMALL_NUMBER)); } }
            SDK(TEXT("read_actual_render_binding"));const FSkeletalMeshModel* const OutputModel = OutputMesh->GetImportedModel();if(!Guard()) { return ErrorResponse(Work->R.Error); }
            const FSkeletalMeshLODModel* const OutputLod = OutputModel && OutputModel->LODModels.IsValidIndex(0)
                ? &OutputModel->LODModels[0]
                : nullptr;
            const FSkelMeshSection* BoundSection = nullptr;
            int32 BoundSectionIndex = INDEX_NONE;
            if (OutputLod)
            {
                for (int32 OutputSectionIndex = 0; OutputSectionIndex < OutputLod->Sections.Num(); ++OutputSectionIndex)
                {
                    const FSkelMeshSection& Candidate = OutputLod->Sections[OutputSectionIndex];
                    if (Candidate.ClothingData.AssetGuid == ClothingAsset->GetAssetGuid() &&
                        Candidate.ClothingData.AssetLodIndex == 0)
                    {
                        BoundSection = &Candidate;
                        BoundSectionIndex = OutputSectionIndex;
                        break;
                    }
                }
            }
            const bool bSourceSectionHasClothMapping = BoundSection && BoundSection->HasClothingData();
            const bool bBoundToRequestedSection = BoundSection &&
                BoundSectionIndex == Request.SectionIndex &&
                BoundSection->OriginalDataSectionIndex == Request.ExpectedOriginalDataSectionIndex;
            const bool bBound = bSourceSectionHasClothMapping && bBoundToRequestedSection &&
                OutputMesh->GetClothingAsset(BoundSection->ClothingData.AssetGuid) == ClothingAsset;
            const bool bRenderSectionUsesCloth = BoundSectionIndex != INDEX_NONE &&
                OutputMesh->IsSectionUsingCloth(BoundSectionIndex, true);
            const bool bConfigValid = ClothingAsset->GetClothConfig<UChaosClothConfig>() != nullptr &&
                ClothingAsset->GetClothConfig<UChaosClothSharedSimConfig>() == SharedConfig;
            const bool bPhysicsAssetSelectionValid = bCreateVroidLowerBodyClothPhysicsAsset
                ? ClothingAsset->PhysicsAsset == GeneratedClothPhysicsAsset
                : bUseSourcePhysicsAsset
                    ? ClothingAsset->PhysicsAsset == Work->SourcePhysics.Get()
                    : ClothingAsset->PhysicsAsset == nullptr;
            const bool bMapValid = AllMapValuesValid&&ObservedMapCount>0&&PinnedCount>0&&DynamicCount>0;
            const auto& ActualMaterials=OutputMesh->GetMaterials();const bool bMaterialValid=BoundSection&&ActualMaterials.IsValidIndex(BoundSection->MaterialIndex)&&ActualMaterials[BoundSection->MaterialIndex].MaterialInterface.Get()==Request.RenderMaterial;
            if (bBound && bConfigValid && bPhysicsAssetSelectionValid && bMapValid && bMaterialValid)
            {
                ++VerifiedBindingCount;
                VerifiedPinnedVertexCount += PinnedCount;
                VerifiedDynamicVertexCount += DynamicCount;
            }
            if (bRenderSectionUsesCloth)
            {
                ++VerifiedRenderBindingCount;
            }

            TSharedPtr<FJsonObject> SectionResult = MakeShared<FJsonObject>();
            SectionResult->SetNumberField(TEXT("section_index"), Request.SectionIndex);
            SectionResult->SetNumberField(TEXT("bound_section_index"), BoundSectionIndex);
            SectionResult->SetStringField(TEXT("material_slot"), Request.ActualMaterialSlot);
            SectionResult->SetStringField(TEXT("material_path"), Request.RenderMaterial->GetPathName());
            SectionResult->SetStringField(TEXT("material_source"), Request.MaterialSource);
            SectionResult->SetStringField(TEXT("clothing_asset_path"), ClothingAsset->GetPathName());
            SectionResult->SetNumberField(TEXT("max_distance_cm"), Request.MaxDistanceCm);
            SectionResult->SetNumberField(TEXT("pinned_vertex_count"), PinnedCount);
            SectionResult->SetNumberField(TEXT("dynamic_vertex_count"), DynamicCount);
            SectionResult->SetStringField(
                TEXT("pin_source"),
                Request.bUseVerticalPinFallback
                    ? TEXT("top 18 percent of local Z range")
                    : TEXT("Cloth_PinGuide vertex-color red channel"));
            SectionResult->SetBoolField(TEXT("bound"), bBound);
            SectionResult->SetBoolField(TEXT("bound_to_requested_section"), bBoundToRequestedSection);
            SectionResult->SetBoolField(TEXT("source_section_has_cloth_mapping"), bSourceSectionHasClothMapping);
            SectionResult->SetBoolField(TEXT("render_section_uses_cloth"), bRenderSectionUsesCloth);
            SectionResult->SetBoolField(TEXT("chaos_config_valid"), bConfigValid);
            SectionResult->SetBoolField(TEXT("physics_asset_selection_valid"), bPhysicsAssetSelectionValid);
            SectionResult->SetStringField(
                TEXT("cloth_physics_asset_path"),
                ClothingAsset->PhysicsAsset ? ClothingAsset->PhysicsAsset->GetPathName() : FString());
            SectionResult->SetBoolField(TEXT("max_distance_map_valid"), bMapValid);SectionResult->SetBoolField(TEXT("actual_output_render_material_matches"),bMaterialValid);SectionResult->SetNumberField(TEXT("observed_physical_weight_count"),ObservedMapCount);
            CreatedSectionResults.Add(MakeShared<FJsonValueObject>(SectionResult));
        }
    }

    const bool bReadbackValid = bCreationSucceeded &&
        VerifiedBindingCount == SectionRequests.Num() &&
        VerifiedRenderBindingCount == SectionRequests.Num() &&
        VerifiedPinnedVertexCount > 0 && VerifiedDynamicVertexCount > 0;
    if (bReadbackValid)
    {
        OutputMesh->MarkPackageDirty();if(!Guard()) { return ErrorResponse(Work->R.Error); }
    }
    const bool bPhysicsAssetSaved = bReadbackValid &&
        (!GeneratedClothPhysicsAsset || SaveObjectIfRequested(GeneratedClothPhysicsAsset, bSaveAssets));
    const bool bSaved = bPhysicsAssetSaved && SaveObjectIfRequested(OutputMesh, bSaveAssets);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    const bool bVerified = bReadbackValid && (!bSaveAssets || bSaved);

    TSharedPtr<FJsonObject> After = MakeShared<FJsonObject>();
    After->SetStringField(TEXT("output_skeletal_mesh_path"), OutputMesh->GetPathName());
    After->SetNumberField(TEXT("clothing_asset_count"), OutputMesh->GetMeshClothingAssets().Num());
    After->SetNumberField(TEXT("verified_binding_count"), VerifiedBindingCount);
    After->SetNumberField(TEXT("verified_render_binding_count"), VerifiedRenderBindingCount);
    After->SetNumberField(TEXT("verified_pinned_vertex_count"), VerifiedPinnedVertexCount);
    After->SetNumberField(TEXT("verified_dynamic_vertex_count"), VerifiedDynamicVertexCount);
    After->SetBoolField(TEXT("use_source_physics_asset"), bUseSourcePhysicsAsset);
    After->SetBoolField(
        TEXT("create_vroid_lower_body_cloth_physics_asset"),
        bCreateVroidLowerBodyClothPhysicsAsset);
    After->SetStringField(
        TEXT("generated_cloth_physics_asset_path"),
        GeneratedClothPhysicsAsset ? GeneratedClothPhysicsAsset->GetPathName() : FString());
    After->SetBoolField(TEXT("generated_cloth_physics_asset_saved"), bPhysicsAssetSaved);
    After->SetArrayField(TEXT("sections"), CreatedSectionResults);
    After->SetBoolField(TEXT("saved"), bSaved);

    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    if(!bVerified) { Work->Fail(TEXT("creation_readback_failed"),CreationError.IsEmpty()?TEXT("Legacy clothing bind/map/config/save verification failed"):CreationError); }
    const bool bRollbackAttempted = !bVerified;
    FOwnedAssetCleanupResult Rollback;
    if (bRollbackAttempted)
    {
        Rollback = RollbackOwnedOutputs();
    }
    const bool bPartialTargetModified = !bVerified && !Rollback.bSucceeded;
    const int32 OwnedOutputCount = 1 + (GeneratedClothPhysicsAsset ? 1 : 0);
    After->SetBoolField(TEXT("rollback_attempted"), bRollbackAttempted);
    After->SetBoolField(TEXT("rollback_succeeded"), bRollbackAttempted && Rollback.bSucceeded);
    After->SetNumberField(TEXT("rollback_requested_asset_count"), Rollback.RequestedCount);
    After->SetNumberField(TEXT("rollback_deleted_asset_count"), Rollback.DeletedCount);

    TSharedPtr<FJsonObject> Delta = MakeShared<FJsonObject>();
    Delta->SetNumberField(TEXT("affected_count"), OwnedOutputCount);
    Delta->SetNumberField(
        TEXT("created_count"),
        bVerified ? OwnedOutputCount : FMath::Max(OwnedOutputCount - Rollback.DeletedCount, 0));
    Delta->SetNumberField(TEXT("updated_count"), 0);
    Delta->SetNumberField(TEXT("removed_count"), Rollback.DeletedCount);
    Delta->SetNumberField(TEXT("skipped_count"), 0);
    Delta->SetNumberField(TEXT("failed_count"), bVerified ? 0 : 1);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bVerified);
    if (!bVerified)
    {
        Result->SetStringField(
            TEXT("error"),
            CreationError.IsEmpty()
                ? TEXT("Legacy Chaos Cloth creation did not pass source/render binding, paint-map, config, or save readback")
                : CreationError);
    }
    Result->SetStringField(TEXT("output_skeletal_mesh_path"), Work->OutputPath);
    Result->SetBoolField(TEXT("save_requested"), bSaveAssets);
    Result->SetBoolField(TEXT("save_attempted"), bReadbackValid && bSaveAssets);
    Result->SetBoolField(TEXT("no_op"), false);
    Result->SetBoolField(TEXT("mutation_attempted"), true);
    Result->SetStringField(TEXT("transaction_scope"), TEXT("owned"));
    Result->SetBoolField(TEXT("rollback_attempted"), bRollbackAttempted);
    Result->SetBoolField(TEXT("rollback_succeeded"), bRollbackAttempted && Rollback.bSucceeded);
    Result->SetNumberField(TEXT("rollback_requested_asset_count"), Rollback.RequestedCount);
    Result->SetNumberField(TEXT("rollback_deleted_asset_count"), Rollback.DeletedCount);
    Result->SetBoolField(TEXT("partial_target_modified"), bPartialTargetModified);
    Result->SetBoolField(TEXT("do_not_retry"), !bVerified);
    FLegacy::AddMutationResultContract(
        Result,
        false,
        true,
        bVerified || bPartialTargetModified,
        Before,
        After,
        Delta,
        bVerified,
        TEXT("legacy_clothing_source_and_render_binding_chaos_config_and_MaxDistance_readback"),
        {TEXT("new_temp_output"), TEXT("source_mesh_unchanged"), TEXT("material_slots"), TEXT("black_and_white_pin_guide"), TEXT("source_section_binding"), TEXT("render_section_binding"), TEXT("chaos_config"), TEXT("physics_asset_selection"), TEXT("save_gate")});
    return Result;
}
}
FString CreateFromSections(const FString& ParamsJson)
{
    FWork W;if(!Parse(ParamsJson,W.Params)) { W.Fail(TEXT("parameters_invalid"),TEXT("Bounded finite unambiguous JSON object required"));return W.Finish(nullptr); }
    if(!IsInGameThread()||!GEditor||Running) { W.Fail(TEXT("editor_required"),TEXT("Non-reentrant Editor game-thread execution required"));return W.Finish(nullptr); }
    TGuardValue<bool> RunningScope(Running,true);TGuardValue<FWork*> WorkScope(Work,&W);W.World=GEditor->GetEditorWorldContext().World();W.WorldCaptured=true;
    auto Result=CreateLegacy(W.Params);bool Success=false;if(Result) { Result->TryGetBoolField(TEXT("success"),Success); }
    W.Transaction.Reset();if(!W.Guard()) { Success=false; }
    if(W.R.bMutationAttempted&&!Success&&!W.Owned.IsEmpty())
    {
        if(W.R.Error.IsEmpty()) { FString E;if(Result) { Result->TryGetStringField(TEXT("error"),E); }W.Fail(TEXT("creation_failed"),E.IsEmpty()?TEXT("Owned legacy clothing creation failed"):E); }
        const auto Cleanup=DeleteOwnedAssetsWithoutConfirmation({});if(!Result) { Result=MakeShared<FJsonObject>(); }Result->SetBoolField(TEXT("success"),false);Result->SetBoolField(TEXT("mutation_attempted"),true);Result->SetBoolField(TEXT("rollback_attempted"),true);Result->SetBoolField(TEXT("rollback_succeeded"),Cleanup.bSucceeded);Result->SetNumberField(TEXT("rollback_requested_asset_count"),Cleanup.RequestedCount);Result->SetNumberField(TEXT("rollback_deleted_asset_count"),Cleanup.DeletedCount);Result->SetBoolField(TEXT("partial_target_modified"),!Cleanup.bSucceeded);Result->SetBoolField(TEXT("do_not_retry"),true);Result->SetStringField(TEXT("output_skeletal_mesh_path"),W.OutputPath);
        if(!Result->HasField(TEXT("contract_version"))) { auto After=MakeShared<FJsonObject>();After->SetStringField(TEXT("output_skeletal_mesh_path"),W.OutputPath);After->SetBoolField(TEXT("rollback_succeeded"),Cleanup.bSucceeded);auto Delta=MakeShared<FJsonObject>();Delta->SetNumberField(TEXT("created_count"),FMath::Max(Cleanup.RequestedCount-Cleanup.DeletedCount,0));Delta->SetNumberField(TEXT("removed_count"),Cleanup.DeletedCount);FLegacy::AddMutationResultContract(Result,false,true,!Cleanup.bSucceeded,W.BeforeSource,After,Delta,false,TEXT("owned_creation_failure_prefix"),{TEXT("exact_owned_identity"),TEXT("reference_checked_cleanup"),TEXT("residual_output_observation")}); }
    }
    return W.Finish(Result);
}
}
