#include "Compatibility/UE58/DomainExpansion/CubelessDomainCommon.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigRuntimeAsset.h"
#include "ControlRigEditorAsset.h"
#include "RigVMEditorAsset.h"
#include "RigVMModel/RigVMClient.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMModel/RigVMLink.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "Units/RigUnit.h"
#include "Rigs/RigHierarchy.h"
#include "Misc/FileHelper.h"
#include "UObject/UObjectIterator.h"

namespace Cubeless::UE58::DomainExpansion
{
namespace
{
bool SupportedRigUnit(UScriptStruct* Type)
{
    return Live(Type) && Type->IsChildOf(FRigUnit::StaticStruct()) && !Type->HasMetaData(TEXT("Deprecated")) &&
        (Type->GetPathName().StartsWith(TEXT("/Script/ControlRigPhysics.")) || Type->GetPathName().StartsWith(TEXT("/Script/ControlRigDynamics.")));
}
bool SameScalarDefault(const URigVMPin* Pin,const FString& Wanted,const FString& Actual)
{
    if(!Live(Pin)){return false;}
    const FString Type=Pin->GetCPPType();
    auto InBounds=[&](double Value){return FMath::IsFinite(Value)&&FMath::Abs(Value)<1.e12&&
        (!Pin->HasMetaData(TEXT("ClampMin"))||Value>=FCString::Atod(*Pin->GetMetaData(TEXT("ClampMin"))))&&
        (!Pin->HasMetaData(TEXT("ClampMax"))||Value<=FCString::Atod(*Pin->GetMetaData(TEXT("ClampMax"))));};
    if(Type==TEXT("float")||Type==TEXT("double")){double A=0,B=0;return LexTryParseString(A,*Wanted)&&LexTryParseString(B,*Actual)&&InBounds(A)&&FMath::IsFinite(B)&&FMath::IsNearlyEqual(A,B,1.e-6*FMath::Max(1.0,FMath::Abs(A)));}
    if(Type==TEXT("int32")||Type==TEXT("int64")){int64 A=0,B=0;return LexTryParseString(A,*Wanted)&&LexTryParseString(B,*Actual)&&(Type!=TEXT("int32")||(A>=MIN_int32&&A<=MAX_int32))&&InBounds(static_cast<double>(A))&&A==B;}
    if(Type==TEXT("bool")){return (Wanted==TEXT("True")||Wanted==TEXT("False")||Wanted==TEXT("true")||Wanted==TEXT("false"))&&Wanted.Equals(Actual,ESearchCase::IgnoreCase);}
    return Wanted==Actual;
}
bool EnglishIdentifier(const FString& Name)
{
    if(Name.IsEmpty()||Name.Len()>64||!BoundedText(Name,64)||HasMCPReference(Name)){return false;}
    for(TCHAR C:Name){if(C>127||(!FChar::IsAlnum(C)&&C!=TEXT('_'))){return false;}}
    return true;
}
bool RigPinPath(const FString& Path)
{
    // Controller paths are FName-backed. Reject oversized tokens before native parsing.
    if(Path.IsEmpty()||!BoundedText(Path,NAME_SIZE-1)||HasMCPReference(Path)){return false;}
    return true;
}
bool FileDigest(const FString& Filename,FString& Out)
{
    TArray<uint8> Bytes;
    if(!FFileHelper::LoadFileToArray(Bytes,*Filename)){return false;}
    uint8 Digest[FSHA1::DigestSize];FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Digest);Out=BytesToHex(Digest,UE_ARRAY_COUNT(Digest));return true;
}
FObject RigCatalog()
{
    FObject R=Result();TArray<TSharedPtr<FJsonValue>> Rows;
    for(TObjectIterator<UScriptStruct> It;It&&Rows.Num()<256;++It)
    {
        if(!SafeContext()){return Error(TEXT("Editor context changed during rig catalog"));}
        if(!SupportedRigUnit(*It)){continue;}
        TStrongObjectPtr<UScriptStruct> Type(*It);const FString Path=Type->GetPathName();
        FStructOnScope Defaults(Type.Get());
        if(!SafeContext()||!SupportedRigUnit(Type.Get())||Type->GetPathName()!=Path){return Error(TEXT("Rig unit retired during native default construction"));}
        FObject Row=MakeShared<FJsonObject>(),Fields=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("struct_path"),Path);Row->SetStringField(TEXT("display_name"),Type->GetMetaData(TEXT("DisplayName")));
        for(TFieldIterator<FProperty> Field(Type.Get());Field;++Field)
        {
            if(!Field->HasMetaData(TEXT("Input"))&&!Field->HasMetaData(TEXT("Output"))){continue;}
            FString Text;Field->ExportTextItem_Direct(Text,Field->ContainerPtrToValuePtr<void>(Defaults.GetStructMemory()),nullptr,nullptr,PPF_None);
            if(!SafeContext()||!SupportedRigUnit(Type.Get())||Type->GetPathName()!=Path){return Error(TEXT("Rig unit retired during default export"));}
            Fields->SetStringField(Field->GetName(),Text);
        }
        Row->SetObjectField(TEXT("pin_defaults"),Fields);Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    R->SetArrayField(TEXT("units"),Rows);R->SetBoolField(TEXT("truncated"),Rows.Num()==256);return R;
}

struct FRigRecipeWork
{
    TStrongObjectPtr<UObject> Source,Candidate,Editor;
    TStrongObjectPtr<UPackage> SourcePackage,Package;
    TStrongObjectPtr<URigHierarchy> Hierarchy;
    TStrongObjectPtr<URigVMGraph> Graph;
    TStrongObjectPtr<URigVMController> Controller;
    FControlRigAssetInterfacePtr Interface;
    FRigVMEditorAssetInterfacePtr VM;
    FString SourcePath,Target,SourceFilename,SourceDigest;
    UClass* SourceClass=nullptr;
    bool BracketOpen=false,Known=true,CompileAttempted=false,CompileReturned=false,AutoCompileDisabled=false;
    FObject R=Result();
    TArray<TSharedPtr<FJsonValue>> Prefix;
    bool SourceCurrent()const
    {
        return SafeContext()&&Live(Source.Get())&&Live(SourcePackage.Get())&&Source->GetClass()==SourceClass&&Source->GetPathName()==SourcePath&&Source->GetOutermost()==SourcePackage.Get()&&!SourcePackage->IsDirty();
    }
    bool CandidateCurrent()const
    {
        return SourceCurrent()&&Live(Candidate.Get())&&Live(Package.Get())&&Candidate->GetClass()==SourceClass&&Candidate->GetOutermost()==Package.Get()&&Package->GetName()==Target&&
            Candidate->GetPathName()==Target+TEXT(".")+FPackageName::GetLongPackageAssetName(Target);
    }
    bool Current()const
    {
        // No cached pin/node is used across a controller callback.
        return CandidateCurrent()&&Live(Editor.Get())&&(Editor.Get()==Candidate.Get()||Editor->IsIn(Candidate.Get()))&&Interface&&VM&&VM->GetObject()==Editor.Get()&&
            Live(Hierarchy.Get())&&Hierarchy->IsIn(Candidate.Get())&&Interface->GetHierarchy()==Hierarchy.Get()&&
            Live(Graph.Get())&&Graph->IsIn(Editor.Get())&&VM->GetDefaultModel()==Graph.Get()&&Live(Controller.Get())&&Controller->GetGraph()==Graph.Get();
    }
    void Enter(const TCHAR* Stage){R->SetStringField(TEXT("stage"),Stage);R->SetBoolField(TEXT("read_only"),false);R->SetBoolField(TEXT("mutation_performed"),true);Prefix.Add(MakeShared<FJsonValueString>(Stage));}
    bool After(){if(!Current()){Known=false;return false;}return true;}
    FObject Fail(const FString& Why)
    {
        // Do not issue an undo or another UObject call after an uncertain callback.
        if(Known&&BracketOpen&&Current())
        {
            Enter(TEXT("cancel_undo_bracket"));const bool Cancelled=Controller->CancelUndoBracket();BracketOpen=false;
            if(After()){R->SetBoolField(TEXT("undo_bracket_cancel_returned"),Cancelled);}else{Known=false;}
        }
        R->SetBoolField(TEXT("success"),false);R->SetStringField(TEXT("error"),Why);R->SetBoolField(TEXT("candidate_retained"),Candidate.IsValid());
        R->SetBoolField(TEXT("change_state_known"),false);R->SetBoolField(TEXT("do_not_retry"),true);R->SetBoolField(TEXT("atomic_rollback_available"),false);
        R->SetBoolField(TEXT("postcondition_verified"),false);R->SetBoolField(TEXT("authored_state_verified"),false);
        R->SetBoolField(TEXT("authored_state_verified_after_publication"),false);
        // Pre-publication rows cannot describe a candidate changed by a later callback.
        R->RemoveField(TEXT("pin_default_readback"));R->RemoveField(TEXT("created_units"));
        R->SetBoolField(TEXT("saved"),false);R->SetArrayField(TEXT("actual_call_prefix"),Prefix);R->SetStringField(TEXT("target_path"),Target);
        R->SetBoolField(TEXT("compile_attempted"),CompileAttempted);R->SetBoolField(TEXT("compile_call_returned"),CompileReturned);R->SetBoolField(TEXT("compilation_pending"),CompileAttempted&&!CompileReturned);
        R->SetBoolField(TEXT("candidate_auto_compile_last_observed_disabled"),AutoCompileDisabled);
        R->SetStringField(TEXT("recovery"),TEXT("Inspect retained candidate and source; no automatic retry or verified rollback"));return R;
    }
};

FObject RigRecipe(const FObject& P)
{
    FRigRecipeWork W;W.Target=String(P,TEXT("target_path"));const FString SourcePath=String(P,TEXT("source_path"));FString Why;
    if(!BoundedObjectPath(SourcePath)||!UnusedAssetPath(W.Target,Why)){return Error(Why.IsEmpty()?TEXT("Bounded source object path required"):Why);}
    W.Source.Reset(LoadObject<UObject>(nullptr,*SourcePath));
    if(!SafeContext()||!IndependentSavedSource(W.Source.Get(),Why)){return Error(Why.IsEmpty()?TEXT("Source load changed editor context"):Why);}
    if(!W.Source->IsA<UControlRigRuntimeAsset>()&&!W.Source->IsA<UControlRigBlueprint>()){return Error(TEXT("Source must be a standalone runtime rig or legacy ControlRig Blueprint"));}
    W.SourcePath=W.Source->GetPathName();W.SourceClass=W.Source->GetClass();W.SourcePackage.Reset(W.Source->GetOutermost());
    if(!FPackageName::DoesPackageExist(W.SourcePackage->GetName(),&W.SourceFilename)||!FileDigest(W.SourceFilename,W.SourceDigest)||!W.SourceCurrent()){return Error(TEXT("Unable to capture clean persisted source"));}
    const TArray<TSharedPtr<FJsonValue>> *Nodes=nullptr,*Links=nullptr;
    if(!P->TryGetArrayField(TEXT("nodes"),Nodes)||!Nodes||Nodes->IsEmpty()||Nodes->Num()>32||!P->TryGetArrayField(TEXT("links"),Links)||!Links||Links->Num()>128){return Error(TEXT("1..32 unit nodes and 0..128 exact links required"));}
    TSet<FString> Names;TMap<FString,TStrongObjectPtr<UScriptStruct>> Types;
    for(const auto& Value:*Nodes)
    {
        if(!Value||Value->Type!=EJson::Object){return Error(TEXT("Unit node object required"));}
        const FObject Spec=Value->AsObject();const FString Name=String(Spec,TEXT("name")),Path=String(Spec,TEXT("struct_path"));
        if(!EnglishIdentifier(Name)||Names.Contains(Name)||!BoundedObjectPath(Path)){return Error(TEXT("Unique bounded engine unit name and struct object path required"));}
        TStrongObjectPtr<UScriptStruct> Type(LoadObject<UScriptStruct>(nullptr,*Path));
        if(!W.SourceCurrent()||!SupportedRigUnit(Type.Get())){return Error(TEXT("Current nondeprecated Physics/Dynamics unit struct required"));}
        if(Spec->HasField(TEXT("pin_defaults")))
        {
            const TSharedPtr<FJsonObject>* Defaults=nullptr;
            if(!Spec->TryGetObjectField(TEXT("pin_defaults"),Defaults)||!Defaults||!Defaults->IsValid()||(*Defaults)->Values.Num()>64){return Error(TEXT("At most 64 pin defaults required"));}
            for(const auto& Pair:(*Defaults)->Values)
            {
                FString Text;
                if(!RigPinPath(Name+TEXT(".")+Pair.Key)||!Pair.Value||!Pair.Value->TryGetString(Text)||!BoundedText(Text,4096)||HasMCPReference(Text)){return Error(TEXT("Bounded scalar pin defaults required"));}
            }
        }
        Names.Add(Name);Types.Add(Name,MoveTemp(Type));
    }
    for(const auto& Value:*Links)
    {
        if(!Value||Value->Type!=EJson::Object||!RigPinPath(String(Value->AsObject(),TEXT("from_pin")))||!RigPinPath(String(Value->AsObject(),TEXT("to_pin")))){return Error(TEXT("Bounded exact link objects required"));}
    }
    W.R->SetBoolField(TEXT("dry_run"),DryRun(P));W.R->SetStringField(TEXT("target_path"),W.Target);W.R->SetBoolField(TEXT("saved"),false);
    if(DryRun(P)){W.R->SetStringField(TEXT("validation_level"),TEXT("source/path/unit-class preflight; controller and compile validate candidate"));return W.R;}
    // Source loads and audits may invoke callbacks. Recheck target immediately before the first writer.
    if(!W.SourceCurrent()||!UnusedAssetPath(W.Target,Why)||!W.SourceCurrent()){return Error(Why.IsEmpty()?TEXT("Source changed before duplication"):Why);}
    W.Enter(TEXT("create_candidate_package"));W.Package.Reset(CreatePackage(*W.Target));
    if(!W.SourceCurrent()||!Live(W.Package.Get())||W.Package->GetName()!=W.Target){W.Known=false;return W.Fail(TEXT("Candidate package creation changed identity/context"));}
    W.Enter(TEXT("duplicate_candidate"));W.Candidate.Reset(DuplicateObject<UObject>(W.Source.Get(),W.Package.Get(),*FPackageName::GetLongPackageAssetName(W.Target)));
    if(!W.CandidateCurrent()){W.Known=false;return W.Fail(TEXT("Candidate duplication failed or source identity changed"));}
    W.Candidate->SetFlags(RF_Public|RF_Standalone|RF_Transactional);
    W.Enter(TEXT("acquire_candidate_editor_asset"));
    W.Editor.Reset(W.Candidate->IsA<UControlRigRuntimeAsset>()?CastChecked<UControlRigRuntimeAsset>(W.Candidate.Get())->GetEditorAsset():W.Candidate.Get());
    if(!W.CandidateCurrent()||!Live(W.Editor.Get())||!W.Editor->GetClass()->ImplementsInterface(UControlRigEditorAssetInterface::StaticClass())||(W.Editor.Get()!=W.Candidate.Get()&&!W.Editor->IsIn(W.Candidate.Get()))){W.Known=false;return W.Fail(TEXT("Candidate must own its editor asset"));}
    W.Interface=FControlRigAssetInterfacePtr(W.Editor.Get());W.VM=W.Interface->GetRigVMAssetInterface();
    if(!W.CandidateCurrent()||!W.VM||W.VM->GetObject()!=W.Editor.Get()){W.Known=false;return W.Fail(TEXT("Candidate RigVM interface identity unavailable"));}
    W.Hierarchy.Reset(W.Interface->GetHierarchy());W.Graph.Reset(W.VM->GetDefaultModel());
    if(!Live(W.Graph.Get())){return W.Fail(TEXT("Candidate default graph required"));}
    W.Controller.Reset(W.VM->GetController(W.Graph.Get()));
    if(!W.After()||W.Graph->GetNodes().Num()>1024){return W.Fail(TEXT("Bounded candidate-owned hierarchy/model/controller required"));}
    const int32 HierarchyCount=W.Hierarchy->Num();const bool AutoCompileBefore=W.VM->GetAutoVMRecompile();W.VM->SetAutoVMRecompile(false);W.AutoCompileDisabled=true;
    W.Enter(TEXT("open_undo_bracket"));const bool Opened=W.Controller->OpenUndoBracket(TEXT("Author UE58 Physics Dynamics Units"));
    if(!W.After()||!Opened){return W.Fail(TEXT("Unable to open native controller bracket"));}W.BracketOpen=true;
    TMap<FString,FString> DefaultsApplied;
    for(int32 I=0;I<Nodes->Num();++I)
    {
        const FObject Spec=(*Nodes)[I]->AsObject();const FString Name=String(Spec,TEXT("name"));UScriptStruct* Type=Types.FindChecked(Name).Get();
        if(!W.Current()||!SupportedRigUnit(Type)||W.Graph->FindNodeByName(*Name)){return W.Fail(TEXT("Unit source retired or node name already exists; replacement refused"));}
        W.Enter(TEXT("add_unit_node"));URigVMUnitNode* Unit=W.Controller->AddUnitNode(Type,TEXT("Execute"),FVector2D(I*260,400),Name,true,false);
        if(!W.After()){return W.Fail(TEXT("Candidate identity changed during AddUnitNode"));}
        if(!Live(Unit)||Unit!=W.Graph->FindNodeByName(*Name)||Unit->GetName()!=Name||Unit->GetScriptStruct()!=Type){return W.Fail(TEXT("Native AddUnitNode failed or renamed/replaced node"));}
        const TSharedPtr<FJsonObject>* Defaults=nullptr;
        if(Spec->TryGetObjectField(TEXT("pin_defaults"),Defaults))for(const auto& Pair:(*Defaults)->Values)
        {
            const FString PinPath=Name+TEXT(".")+Pair.Key,Text=Pair.Value->AsString();const URigVMPin* Pin=W.Graph->FindPin(PinPath);
            if(!Live(Pin)||Pin->IsUObject()||Pin->IsArray()||Pin->IsStruct()||Pin->IsExecuteContext()||(Pin->GetDirection()!=ERigVMPinDirection::Input&&Pin->GetDirection()!=ERigVMPinDirection::IO)||
                ((Pin->GetCPPType()!=TEXT("FString")&&Pin->GetCPPType()!=TEXT("FText"))&&Text.Len()>=NAME_SIZE)||!SameScalarDefault(Pin,Text,Text)){return W.Fail(TEXT("Bounded scalar input leaf required; object/array/struct defaults refused"));}
            W.Enter(TEXT("set_pin_default"));const bool Accepted=W.Controller->SetPinDefaultValue(PinPath,Text,false,true,false,false,false);
            if(!W.After()){return W.Fail(TEXT("Candidate identity changed during SetPinDefaultValue"));}Pin=W.Graph->FindPin(PinPath);
            if(!Accepted||!SameScalarDefault(Pin,Text,Pin?Pin->GetDefaultValue():FString())){return W.Fail(TEXT("Native scalar pin default write/readback failed"));}
            DefaultsApplied.Add(PinPath,Pin->GetDefaultValue());
        }
    }
    for(const auto& Value:*Links)
    {
        const FObject Link=Value->AsObject();const FString From=String(Link,TEXT("from_pin")),To=String(Link,TEXT("to_pin"));
        URigVMPin* First=W.Graph->FindPin(From);URigVMPin* Second=W.Graph->FindPin(To);
        if(!Live(First)||!Live(Second)||!Second->GetLinks().IsEmpty()||(First->IsExecuteContext()&&!First->GetLinks().IsEmpty())){return W.Fail(TEXT("Exact unoccupied target/execute pins required"));}
        W.Enter(TEXT("add_link"));const bool Accepted=W.Controller->AddLink(From,To,true,false,ERigVMPinDirection::Output,false);
        if(!W.After()){return W.Fail(TEXT("Candidate identity changed during AddLink"));}
        First=W.Graph->FindPin(From);Second=W.Graph->FindPin(To);bool Found=false;
        if(Live(First)&&Live(Second))for(const URigVMLink* Existing:Second->GetLinks()){if(Live(Existing)&&Existing->GetSourcePin()==First&&Existing->GetTargetPin()==Second){Found=true;break;}}
        if(!Accepted||!Found){return W.Fail(TEXT("Native schema link write/readback failed"));}
    }
    auto RequestedLinksPresent=[&]
    {
        if(!W.Current()){return false;}
        for(const auto& Value:*Links)
        {
            const FObject Link=Value->AsObject();const FString From=String(Link,TEXT("from_pin")),To=String(Link,TEXT("to_pin"));
            const URigVMPin* First=W.Graph->FindPin(From);const URigVMPin* Second=W.Graph->FindPin(To);
            if(!Live(First)||!Live(Second)||Second->GetLinks().Num()!=1||(First->IsExecuteContext()&&First->GetLinks().Num()!=1)){return false;}
            const URigVMLink* Actual=Second->GetLinks()[0];
            if(!Live(Actual)||Actual->GetSourcePin()!=First||Actual->GetTargetPin()!=Second||!First->GetLinks().Contains(Actual)){return false;}
        }
        return W.Current();
    };
    auto VerifyAuthored=[&]
    {
        if(!W.Current())return false;
        FObject DefaultReadback=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Rows;
        auto Mismatch=[&](const FString& Kind,const FString& Path,const FString& Expected,const FString& Actual)
        {
            W.R->SetStringField(TEXT("authored_mismatch_kind"),Kind);W.R->SetStringField(TEXT("authored_mismatch_path"),Path);
            W.R->SetStringField(TEXT("authored_mismatch_expected"),Expected);W.R->SetStringField(TEXT("authored_mismatch_actual"),Actual);return false;
        };
        for(const auto& Pair:DefaultsApplied)
        {
            // Reacquire every pin from the retained current graph after callback boundaries.
            const URigVMPin* Pin=W.Graph->FindPin(Pair.Key);
            if(!Live(Pin))return Mismatch(TEXT("pin_default"),Pair.Key,Pair.Value,TEXT("<missing>"));
            const FString Actual=Pin->GetDefaultValue();
            if(Actual!=Pair.Value)return Mismatch(TEXT("pin_default"),Pair.Key,Pair.Value,Actual);
            DefaultReadback->SetStringField(Pair.Key,Actual);
        }
        for(const FString& Name:Names)
        {
            UScriptStruct* ExpectedType=Types.FindChecked(Name).Get();
            if(!SupportedRigUnit(ExpectedType))return Mismatch(TEXT("unit_type"),Name,TEXT("<captured native unit>"),TEXT("<retired>"));
            auto* Unit=Cast<URigVMUnitNode>(W.Graph->FindNodeByName(*Name));
            if(!Live(Unit))return Mismatch(TEXT("unit"),Name,ExpectedType->GetPathName(),TEXT("<missing or non-unit>"));
            UScriptStruct* ActualType=Unit->GetScriptStruct();
            if(!SupportedRigUnit(ExpectedType)||ActualType!=ExpectedType||!SupportedRigUnit(ActualType))
                return Mismatch(TEXT("unit_type"),Name,Live(ExpectedType)?ExpectedType->GetPathName():TEXT("<retired>"),Live(ActualType)?ActualType->GetPathName():TEXT("<missing or retired>"));
            FObject Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("node"),Unit->GetNodePath());Row->SetStringField(TEXT("struct_path"),ActualType->GetPathName());Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        if(!W.Current())return false;
        W.R->SetObjectField(TEXT("pin_default_readback"),DefaultReadback);W.R->SetArrayField(TEXT("created_units"),Rows);return true;
    };
    W.Enter(TEXT("close_undo_bracket"));const bool Closed=W.Controller->CloseUndoBracket();
    if(!W.After()){return W.Fail(TEXT("Candidate identity changed closing bracket"));}W.BracketOpen=false;if(!Closed){return W.Fail(TEXT("Unable to close controller bracket"));}
    W.Enter(TEXT("compile_rig_vm"));W.CompileAttempted=true;W.VM->RecompileVM();W.CompileReturned=true;
    if(!W.After()){return W.Fail(TEXT("Candidate identity changed compiling RigVM"));}
    W.R->SetNumberField(TEXT("compile_errors"),W.VM->GetCompileLog().NumErrors);
    if(W.VM->GetCompileLog().NumErrors>0||!W.VM->GetVM(false)||W.Hierarchy->Num()!=HierarchyCount){return W.Fail(TEXT("RigVM compilation/hierarchy verification failed"));}
    W.R->SetStringField(TEXT("stage"),TEXT("verify_requested_links_after_compile"));
    if(!RequestedLinksPresent()){return W.Fail(TEXT("Requested RigVM link endpoints changed during compilation callbacks"));}
    W.VM->SetAutoVMRecompile(AutoCompileBefore);W.AutoCompileDisabled=!AutoCompileBefore;
    W.R->SetStringField(TEXT("stage"),TEXT("verify_authored_after_compile"));
    if(!VerifyAuthored()){return W.Fail(TEXT("Authored RigVM pin default or unit changed during compilation callbacks"));}
    W.R->SetBoolField(TEXT("authored_state_verified_after_compile"),true);W.R->SetBoolField(TEXT("compile_succeeded"),true);
    W.Enter(TEXT("publish_candidate"));PublishCandidate(W.Candidate.Get(),W.R);
    if(!W.After()){return W.Fail(TEXT("Candidate/source identity changed during publication"));}
    if(!W.R->GetBoolField(TEXT("success"))){return W.Fail(String(W.R,TEXT("error")));}
    W.R->SetStringField(TEXT("stage"),TEXT("verify_requested_links_after_publication"));
    if(!RequestedLinksPresent()){return W.Fail(TEXT("Requested RigVM link endpoints changed during publication callbacks"));}
    W.R->SetBoolField(TEXT("requested_links_verified_after_publication"),true);
    W.R->SetStringField(TEXT("stage"),TEXT("verify_authored_after_publication"));
    if(!VerifyAuthored()){return W.Fail(TEXT("Authored RigVM pin default or unit changed during publication callbacks"));}
    W.R->SetBoolField(TEXT("authored_state_verified"),true);W.R->SetBoolField(TEXT("authored_state_verified_after_publication"),true);
    FString DigestAfter;if(!FileDigest(W.SourceFilename,DigestAfter)||DigestAfter!=W.SourceDigest){return W.Fail(TEXT("Persisted source bytes changed; candidate retained"));}
    W.R->SetBoolField(TEXT("source_saved_file_unchanged"),true);W.R->SetBoolField(TEXT("source_identity_unchanged"),true);W.R->SetBoolField(TEXT("source_package_remained_clean"),true);
    W.R->SetStringField(TEXT("source_preservation_scope"),TEXT("Exact object/package identity, clean package and persisted source bytes; arbitrary unmarked in-memory changes not proven"));
    W.R->SetArrayField(TEXT("actual_call_prefix"),W.Prefix);return W.R;
}
}
FObject ExecuteRig(const FObject& P)
{
    if(!P||!SafeContext()||(P->HasField(TEXT("dry_run"))&&!P->HasTypedField<EJson::Boolean>(TEXT("dry_run")))){return Error(TEXT("Bounded editor game-thread request with boolean dry_run required"));}
    const FString Operation=String(P,TEXT("operation"));if(Operation==TEXT("catalog")){return RigCatalog();}if(Operation==TEXT("recipe")){return RigRecipe(P);}return Error(TEXT("Expected rig operation catalog or recipe"));
}
}
