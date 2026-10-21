#include "Compatibility/UE58/CubelessBlendSpaceVariantApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/ReferencerFinder.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::BlendSpaceSampleVariant
{
namespace
{
using Variable::FResult;
constexpr int32 MaxCarrier=1024*1024,MaxRows=4096,MaxRefs=65536,MaxGrid=256;
bool Running=false;
bool Live(const UObject* Object){return IsValid(Object)&&!Object->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool Safe(const FString& Text,int32 Limit=MaxCarrier){if(Text.Len()>Limit){return false;}for(TCHAR Ch:Text){if(Ch==0){return false;}}return true;}
bool Decode(const FString& Text,TSharedPtr<FJsonValue>& Out)
{
    if(Text.IsEmpty()||!Safe(Text)){return false;}bool Quote=false,Escape=false;int32 Depth=0,Slots=0;
    for(TCHAR Ch:Text)
    {
        if(Quote){if(Escape){Escape=false;}else if(Ch==TEXT('\\')){Escape=true;}else if(Ch==TEXT('"')){Quote=false;}continue;}
        if(Ch==TEXT('"')){Quote=true;}else if(Ch==TEXT('[')||Ch==TEXT('{')){if(++Depth>64||++Slots>MaxRefs){return false;}}
        else if(Ch==TEXT(']')||Ch==TEXT('}')){if(--Depth<0){return false;}}else if(Ch==TEXT(',')&&++Slots>MaxRefs){return false;}
    }
    TArray<TSharedPtr<FJsonValue>> Root;if(Quote||Depth!=0||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),Root)||Root.Num()!=1){return false;}
    Out=Root[0];TArray<TSharedPtr<FJsonValue>> Stack={Out};int32 Count=0;
    while(!Stack.IsEmpty())
    {
        auto Value=Stack.Pop(EAllowShrinking::No);if(!Value.IsValid()||++Count>MaxRefs){return false;}
        if(Value->Type==EJson::Number&&!FMath::IsFinite(Value->AsNumber())){return false;}
        if(Value->Type==EJson::Array){Stack.Append(Value->AsArray());}else if(Value->Type==EJson::Object){for(const auto& Pair:Value->AsObject()->Values){Stack.Add(Pair.Value);}}
        if(Stack.Num()>MaxRefs){return false;}
    }
    return Out.IsValid()&&(Out->Type!=EJson::Array||Out->AsArray().Num()<=MaxRows);
}
FString PackagePath(const FString& Raw)
{
    FString Value=FPackageName::ExportTextPathToObjectPath(Raw).TrimStartAndEnd();Value.TrimQuotesInline();Value.ReplaceInline(TEXT("\\"),TEXT("/"));
    int32 Dot=INDEX_NONE;if(Value.FindChar(TEXT('.'),Dot)){Value.LeftInline(Dot);}while(Value.Contains(TEXT("//"))){Value.ReplaceInline(TEXT("//"),TEXT("/"));}
    while(Value.EndsWith(TEXT("/"))){Value.LeftChopInline(1);}return Value;
}
FString Segment(FString Value){for(TCHAR& Ch:Value){if(!FChar::IsAlnum(Ch)&&Ch!=TEXT('_')){Ch=TEXT('_');}}Value.TrimStartAndEndInline();return Value.IsEmpty()?TEXT("AnimNodeProbe"):Value;}
bool GamePath(const FString& Path){return Safe(Path,NAME_SIZE-1)&&Path.StartsWith(TEXT("/Game/"),ESearchCase::CaseSensitive)&&FPackageName::IsValidLongPackageName(Path)&&Safe(Path+TEXT(".")+FPackageName::GetShortName(Path),NAME_SIZE-1);}
bool SamplePath(const FString& Path){for(const TCHAR* Root:{TEXT("/Game/_MCP_Sample"),TEXT("/Game/_MCP_Temp"),TEXT("/Game/CubelessMCPTests")}){if(Path.Equals(Root,ESearchCase::IgnoreCase)||Path.StartsWith(FString(Root)+TEXT("/"),ESearchCase::IgnoreCase)){return true;}}return false;}
bool Forbidden(const FString& Path)
{
    for(const TCHAR* Name:{TEXT("CubelessMCP_UE58"),TEXT("ModelContextProtocol"),TEXT("Toolsets"),TEXT("UnrealMCP")})
    {
        if(Path.Contains(FString(TEXT("/"))+Name+TEXT("/"),ESearchCase::IgnoreCase)){return true;}
        if(auto Plugin=IPluginManager::Get().FindPlugin(Name)){for(const auto& Module:Plugin->GetDescriptor().Modules){if(Path.Contains(FString(TEXT("/Script/"))+Module.Name.ToString(),ESearchCase::IgnoreCase)){return true;}}}
    }
    return Path.Contains(TEXT("/Script/CubelessMCPUE58Editor"),ESearchCase::IgnoreCase);
}
struct FIdentity
{
    TStrongObjectPtr<UObject> Object,Class,Outer,Package;FString Path,PackageName;
    explicit FIdentity(UObject* Value):Object(Value),Class(Value->GetClass()),Outer(Value->GetOuter()),Package(Value->GetPackage()),Path(Value->GetPathName()),PackageName(Value->GetPackage()->GetName()){}
    bool Current()const{return Live(Object.Get())&&Live(Class.Get())&&Live(Package.Get())&&!CastChecked<UClass>(Class.Get())->HasAnyClassFlags(CLASS_NewerVersionExists)&&Object->GetClass()==Class.Get()&&Object->GetOuter()==Outer.Get()&&Object->GetPackage()==Package.Get()&&Object->GetPathName()==Path&&Package->GetPathName()==PackageName;}
};
struct FWork
{
    FResult Result;TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Counts=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Errors,Warnings,AxisResults,SampleResults,AddResults;
    TArray<TUniquePtr<FIdentity>> Ids;TSet<UObject*> Seen;UBlendSpace* Source=nullptr;UBlendSpace* Target=nullptr;bool InPlace=false,SourceDirty=false,ObservedChange=false,EditReportedChange=false,Created=false,Reused=false;
    FString TargetPackage;bool ProtectSourceValues=false;TArray<FBlendParameter> ProtectedAxes;TArray<FBlendSample> ProtectedSamples;
    FWork(bool Save){Result.bSaveRequested=Save;Result.Data->SetObjectField(TEXT("legacy_result"),Legacy);Result.Data->SetObjectField(TEXT("native_call_counts"),Counts);Result.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);Result.Data->SetBoolField(TEXT("atomic_rollback_supported"),false);Result.Data->SetStringField(TEXT("effect_scope"),TEXT("Explicit work counters are separate from opaque load/duplicate/asset callbacks; total SDK effects remain unknown after the first SDK call."));}
    void Count(const TCHAR* Key){double Value=0;Counts->TryGetNumberField(Key,Value);Counts->SetNumberField(Key,Value+1);}
    void SDK(const TCHAR* Key){Result.bChangeStateKnown=false;Count(Key);}
    bool Fail(const TCHAR* Code,const FString& Error){if(Result.Error.IsEmpty()){Result.ErrorCode=FString(TEXT("blendspace_variant_"))+Code;Result.Error=Error;}return false;}
    bool Hold(UObject* Object)
    {
        if(!Object){return true;}if(!Live(Object)||!Live(Object->GetClass())||!Live(Object->GetPackage())){return Fail(TEXT("source_unreadable"),TEXT("A source object/class/actual package is unreadable"));}
        if(Seen.Contains(Object)){return true;}if(Seen.Num()>=MaxRefs){return Fail(TEXT("source_bound"),TEXT("Strong source inventory exceeds65536"));}Seen.Add(Object);Ids.Add(MakeUnique<FIdentity>(Object));return true;
    }
    bool Current()
    {
        if(!Result.Error.IsEmpty()){return false;}if(!IsEditorGameThread()||IsEngineExitRequested()||IsPlaySessionActive()){return Fail(TEXT("editor_boundary"),TEXT("Idle Editor game thread required for asset writes"));}
        for(const auto& Id:Ids){if(!Id->Current()){return Fail(TEXT("source_identity_changed"),TEXT("Source/target object, class, outer, actual package or path changed after callback"));}}
        if(ProtectSourceValues&&!InPlace)
        {
            bool Same=Source->GetPackage()->IsDirty()==SourceDirty&&Source->GetBlendSamples()==ProtectedSamples;
            for(int32 I=0;I<3;++I){const auto& A=Source->GetBlendParameter(I);const auto& B=ProtectedAxes[I];Same&=A.DisplayName==B.DisplayName&&A.Min==B.Min&&A.Max==B.Max&&A.GridNum==B.GridNum&&A.bSnapToGrid==B.bSnapToGrid&&A.bWrapInput==B.bWrapInput;}
            if(!Same){ObservedChange=true;return Fail(TEXT("protected_source_changed"),TEXT("Clone-mode source axes/samples/dirty state changed during a callback; no rollback or retry"));}
        }
        return true;
    }
    void RowError(const TSharedRef<FJsonObject>& Row,const FString& Error){Row->SetBoolField(TEXT("success"),false);Row->SetStringField(TEXT("error"),Error);Errors.Add(MakeShared<FJsonValueString>(Error));}
    FResult Finish()
    {
        Result.Data->SetBoolField(TEXT("observed_explicit_change"),ObservedChange);Result.bChanged|=ObservedChange;
        Legacy->SetBoolField(TEXT("target_blendspace_created"),Created);Legacy->SetBoolField(TEXT("target_blendspace_reused"),Reused);Legacy->SetBoolField(TEXT("original_assets_modified"),InPlace);
        Result.Data->SetBoolField(TEXT("legacy_original_assets_modified_is_in_place_mode"),true);
        Legacy->SetArrayField(TEXT("axis_edit_results"),AxisResults);Legacy->SetArrayField(TEXT("sample_edit_results"),SampleResults);Legacy->SetArrayField(TEXT("add_sample_results"),AddResults);
        if(!Result.Error.IsEmpty()){Legacy->SetStringField(TEXT("error"),Result.Error);}Legacy->SetBoolField(TEXT("success"),Result.bSuccess);Legacy->SetArrayField(TEXT("errors"),Errors);Legacy->SetArrayField(TEXT("warnings"),Warnings);return Result;
    }
};
TSharedRef<FJsonObject> VectorJson(const FVector& V){auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("x"),V.X);J->SetNumberField(TEXT("y"),V.Y);J->SetNumberField(TEXT("z"),V.Z);J->SetArrayField(TEXT("vector"),{MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)});return J;}
TSharedRef<FJsonObject> AxisJson(const FBlendParameter& P,int32 Index)
{
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("index"),Index);J->SetStringField(TEXT("display_name"),P.DisplayName);J->SetNumberField(TEXT("min"),P.Min);J->SetNumberField(TEXT("max"),P.Max);J->SetNumberField(TEXT("grid_num"),P.GridNum);J->SetNumberField(TEXT("grid_size"),P.GridNum>0?P.GetGridSize():0);J->SetBoolField(TEXT("snap_to_grid"),P.bSnapToGrid);J->SetBoolField(TEXT("wrap_input"),P.bWrapInput);return J;
}
TSharedRef<FJsonObject> SampleJson(const FBlendSample& S,int32 Index)
{
    auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("index"),Index);J->SetStringField(TEXT("animation"),S.Animation?S.Animation->GetPathName():FString());J->SetStringField(TEXT("animation_name"),S.Animation?S.Animation->GetName():FString());J->SetObjectField(TEXT("sample_value"),VectorJson(S.SampleValue));J->SetNumberField(TEXT("rate_scale"),S.RateScale);J->SetBoolField(TEXT("use_single_frame_for_blending"),S.bUseSingleFrameForBlending);J->SetNumberField(TEXT("frame_index_to_sample"),S.FrameIndexToSample);return J;
}
bool Inventory(UBlendSpace* Asset,FWork& Work)
{
    if(!Work.Current()||!Work.Hold(Asset)){return false;}
    for(int32 Axis=0;Axis<3;++Axis)
    {
        const auto P=Asset->GetBlendParameter(Axis);const float Step=(P.Max-P.Min)/P.GridNum;if(!FMath::IsFinite(P.Min)||!FMath::IsFinite(P.Max)||P.Min>=P.Max||P.GridNum<1||P.GridNum>MaxGrid||!FMath::IsFinite(Step)||Step<=0||!Safe(P.DisplayName))
        {return Work.Fail(TEXT("asset_axis_bound"),TEXT("BlendSpace axes require finite ordered ranges and grid1..256 before public sampling/resampling"));}
    }
    const auto& Samples=Asset->GetBlendSamples();if(Samples.Num()>MaxRows){return Work.Fail(TEXT("asset_sample_bound"),TEXT("BlendSpace sample inventory exceeds4096"));}
    for(const auto& S:Samples){if(S.SampleValue.ContainsNaN()||!FMath::IsFinite(S.RateScale)||!Work.Hold(S.Animation)){return Work.Fail(TEXT("asset_sample_unreadable"),TEXT("BlendSpace sample animation or values are unreadable"));}}
    return Work.Current();
}
FString Snapshot(UBlendSpace* Asset,FWork& Work,TArray<TSharedPtr<FJsonValue>>& Axes,TArray<TSharedPtr<FJsonValue>>& Samples)
{
    if(!Inventory(Asset,Work)){return {};}
    for(int32 Index=0;Index<3;++Index){Axes.Add(MakeShared<FJsonValueObject>(AxisJson(Asset->GetBlendParameter(Index),Index)));}
    const auto Copy=Asset->GetBlendSamples();for(int32 Index=0;Index<Copy.Num();++Index){Samples.Add(MakeShared<FJsonValueObject>(SampleJson(Copy[Index],Index)));}
    auto J=MakeShared<FJsonObject>();J->SetArrayField(TEXT("axes"),Axes);J->SetArrayField(TEXT("samples"),Samples);FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return Text;
}
UObject* Load(const FString& Raw,FWork& Work,bool bRequiredPath=true)
{
    FString Path=FPackageName::ExportTextPathToObjectPath(Raw).TrimStartAndEnd();Path.TrimQuotesInline();if(!Safe(Path,NAME_SIZE-1)||!FPackageName::IsValidLongPackageName(PackagePath(Path))){if(bRequiredPath){Work.Fail(TEXT("load_path_invalid"),TEXT("Asset path must contain a valid bounded long package name"));}return nullptr;}
    if(!Path.Contains(TEXT("."))){Path+=TEXT(".")+FPackageName::GetShortName(Path);}if(!Safe(Path,NAME_SIZE-1)){if(bRequiredPath){Work.Fail(TEXT("load_path_invalid"),TEXT("Object path exceeds FName limit"));}return nullptr;}
    Work.SDK(TEXT("asset_load_calls"));UObject* Object=StaticLoadObject(UObject::StaticClass(),nullptr,*Path);if(!Work.Current()||!Work.Hold(Object)){return nullptr;}return Object;
}
bool ClassAllowed(UObject* Object,FWork& Work)
{
    if(!Live(Object)||Forbidden(Object->GetPathName())){return Work.Fail(TEXT("runtime_dependency_refused"),TEXT("Asset is unreadable or references an integration content namespace"));}
    TSet<UClass*> Seen;for(UClass* Class=Object->GetClass();Class;Class=Class->GetSuperClass())
    {if(Seen.Num()>=256||Seen.Contains(Class)||!Live(Class)||Forbidden(Class->GetPathName())||!Work.Hold(Class)){return Work.Fail(TEXT("runtime_dependency_refused"),TEXT("Class lineage contains an MCP/integration runtime dependency or is unreadable"));}Seen.Add(Class);}return Work.Current();
}
bool RuntimeGuard(UBlendSpace* Asset,FWork& Work)
{
    if(!Inventory(Asset,Work)||!ClassAllowed(Asset,Work)){return false;}
    if(Asset->GetSkeleton()&&!ClassAllowed(Asset->GetSkeleton(),Work)){return false;}
    for(const FBlendSample& Item:Asset->GetBlendSamples()){if(Item.Animation&&!ClassAllowed(Item.Animation,Work)){return false;}}
    Work.Result.Data->SetBoolField(TEXT("blendspace_skeleton_animation_class_projection_verified"),true);Work.Result.Data->SetBoolField(TEXT("complete_runtime_dependency_closure_verified"),false);return Work.Current();
}
bool OverwriteSafe(UObject* Existing,FWork& Work)
{
    if(Work.SourceDirty){return Work.Fail(TEXT("overwrite_source_dirty"),TEXT("Overwrite refuses a dirty protected source before saved reference admission"));}
    Work.SDK(TEXT("overwrite_registry_acquisition"));auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();if(!Work.Current()||Registry.IsLoadingAssets()){return Work.Fail(TEXT("overwrite_registry_pending"),TEXT("Overwrite needs completed saved reference discovery; no retry was requested"));}
    TArray<FName> Refs;if(!Registry.GetReferencers(FName(*Work.TargetPackage),Refs,UE::AssetRegistry::EDependencyCategory::Package)||Refs.Num()>MaxRefs){return Work.Fail(TEXT("overwrite_reference_unknown"),TEXT("Target package reference projection is unavailable or bounded out"));}
    for(FName Ref:Refs){if(!Ref.ToString().Equals(Work.TargetPackage,ESearchCase::IgnoreCase)){return Work.Fail(TEXT("overwrite_external_reference"),TEXT("Force-delete could affect a package outside the requested target"));}}
    TArray<UObject*> Members;GetObjectsWithPackage(Existing->GetPackage(),Members,EGetObjectsFlags::IncludeNestedObjects,RF_NoFlags,EInternalObjectFlags::Garbage);if(Members.Num()>MaxRows){return Work.Fail(TEXT("overwrite_member_bound"),TEXT("Target package contains over4096 live members"));}
    TArray<TStrongObjectPtr<UObject>> Held;for(UObject* Member:Members){if(!Live(Member)){return Work.Fail(TEXT("overwrite_member_unreadable"),TEXT("Target package member is unavailable"));}Held.Emplace(Member);}Members.AddUnique(Existing);
    Work.SDK(TEXT("overwrite_memory_referencers"));const auto Memory=FReferencerFinder::GetAllReferencers(Members,nullptr,EReferencerFinderFlags::SkipWeakReferences);if(!Work.Current()||Memory.Num()>MaxRefs){return Work.Fail(TEXT("overwrite_reference_unknown"),TEXT("Loaded reference projection changed or exceeds65536"));}
    for(UObject* Ref:Memory){if(!Live(Ref)||!Live(Ref->GetPackage())){return Work.Fail(TEXT("overwrite_reference_unknown"),TEXT("Loaded target referencer is unreadable"));}if(Ref->GetPackage()!=GetTransientPackage()&&!Ref->GetPackage()->GetName().Equals(Work.TargetPackage,ESearchCase::IgnoreCase)){return Work.Fail(TEXT("overwrite_external_reference"),TEXT("Force-delete could clear a reference in a protected loaded package"));}}
    Work.Result.Data->SetBoolField(TEXT("force_delete_may_clear_transient_editor_undo_references"),true);return Work.Current();
}
bool NumberFloat(const TSharedRef<FJsonObject>& Object,const TCHAR* Name,float& Value,bool& Present)
{double N=0;if(!Object->TryGetNumberField(Name,N)){return true;}Present=true;if(!FMath::IsFinite(N)||FMath::Abs(N)>TNumericLimits<float>::Max()){return false;}Value=static_cast<float>(N);return true;}
bool VectorValue(const TSharedPtr<FJsonValue>& Value,FVector& Out)
{
    if(!Value.IsValid()){return false;}double XYZ[3]={0,0,0};
    if(Value->Type==EJson::Array){const auto& A=Value->AsArray();if(A.Num()<3){return false;}for(int32 I=0;I<3;++I){if(!A[I].IsValid()||A[I]->Type!=EJson::Number||!A[I]->TryGetNumber(XYZ[I])){return false;}}}
    else if(Value->Type==EJson::Object){const auto O=Value->AsObject();for(int32 I=0;I<3;++I){if(!O->TryGetNumberField(I==0?TEXT("x"):(I==1?TEXT("y"):TEXT("z")),XYZ[I])&&!O->TryGetNumberField(I==0?TEXT("X"):(I==1?TEXT("Y"):TEXT("Z")),XYZ[I])){return false;}}}
    else{return false;}
    for(double N:XYZ){if(!FMath::IsFinite(N)||FMath::Abs(N)>TNumericLimits<float>::Max()){return false;}}Out=FVector(XYZ[0],XYZ[1],XYZ[2]);return true;
}
bool EditVector(const TSharedRef<FJsonObject>& Object,const FVector& Default,bool Required,FVector& Out,bool& Present,FString& Error)
{
    Out=Default;Present=false;for(const TCHAR* Key:{TEXT("input"),TEXT("position"),TEXT("sample_value")}){auto Value=Object->TryGetField(Key);if(Value.IsValid()){Present=true;if(!VectorValue(Value,Out)){Error=FString::Printf(TEXT("Field '%s' must be a vector array or object"),Key);return false;}return true;}}
    double Number=0;for(int32 I=0;I<3;++I){if(Object->TryGetNumberField(I==0?TEXT("x"):(I==1?TEXT("y"):TEXT("z")),Number)){if(!FMath::IsFinite(Number)||FMath::Abs(Number)>TNumericLimits<float>::Max()){Error=TEXT("Sample coordinates exceed finite float range");return false;}Out[I]=Number;Present=true;}}
    if(Required&&!Present){Error=TEXT("Sample edit requires input/position/sample_value or x/y/z");return false;}return true;
}
bool ExpansionSafe(UBlendSpace* Asset,const FVector& Position,FString& Error)
{
    for(int32 Axis=0;Axis<3;++Axis)
    {
        const auto P=Asset->GetBlendParameter(Axis);const float Delta=(P.Max-P.Min)/P.GridNum;float Minimum=P.Min,Maximum=P.Max;int32 Grid=P.GridNum;
        if(!FMath::IsFinite(Delta)||Delta<=0){Error=TEXT("Axis expansion requires a positive finite float step");return false;}
        while(Position[Axis]>Maximum)
        {const float Next=Maximum+Delta;if(++Grid>MaxGrid||!FMath::IsFinite(Next)||Next<=Maximum){Error=TEXT("Sample expansion is non-progressing or exceeds grid256; no SDK edit was dispatched");return false;}Maximum=Next;}
        while(Position[Axis]<Minimum)
        {const float Next=Minimum-Delta;if(++Grid>MaxGrid||!FMath::IsFinite(Next)||Next>=Minimum){Error=TEXT("Sample expansion is non-progressing or exceeds grid256; no SDK edit was dispatched");return false;}Minimum=Next;}
    }
    return true;
}
UAnimSequence* Animation(const TSharedRef<FJsonObject>& Row,std::initializer_list<const TCHAR*> Keys,FString& Requested,FWork& Work)
{for(const TCHAR* Key:Keys){FString Path;if(Row->TryGetStringField(Key,Path)){Path.TrimStartAndEndInline();if(!Path.IsEmpty()){Requested=Path;return Cast<UAnimSequence>(Load(Path,Work,false));}}}return nullptr;}
int32 AxisIndex(UBlendSpace* Asset,const TSharedRef<FJsonObject>& Row)
{
    double Number=0;if(Row->TryGetNumberField(TEXT("index"),Number)||Row->TryGetNumberField(TEXT("axis_index"),Number)){if(!FMath::IsFinite(Number)||Number<-1||Number>3){return INDEX_NONE;}return FMath::RoundToInt(Number);}
    FString Name;Row->TryGetStringField(TEXT("axis"),Name);Name.TrimStartAndEndInline();for(int32 I=0;I<3;++I){if(Name.Equals(I==0?TEXT("x"):(I==1?TEXT("y"):TEXT("z")),ESearchCase::IgnoreCase)||Name==LexToString(I)||(!Name.IsEmpty()&&Asset->GetBlendParameter(I).DisplayName.Equals(Name,ESearchCase::IgnoreCase))){return I;}}return INDEX_NONE;
}
int32 SampleIndex(UBlendSpace* Asset,const TSharedRef<FJsonObject>& Row,FString& Error)
{
    double Number=0;if(Row->TryGetNumberField(TEXT("index"),Number)||Row->TryGetNumberField(TEXT("sample_index"),Number)){if(!FMath::IsFinite(Number)||Number<-1||Number>MaxRows+1){Error=TEXT("Sample index out of range");return INDEX_NONE;}int32 Index=FMath::RoundToInt(Number);if(!Asset->GetBlendSamples().IsValidIndex(Index)){Error=FString::Printf(TEXT("Sample index out of range: %d"),Index);return INDEX_NONE;}return Index;}
    FString Selector;if(!Row->TryGetStringField(TEXT("animation"),Selector)){Row->TryGetStringField(TEXT("animation_name"),Selector);}Selector.TrimStartAndEndInline();if(Selector.IsEmpty()){Error=TEXT("Sample edit requires index/sample_index or animation/animation_name selector");return INDEX_NONE;}
    const auto Copy=Asset->GetBlendSamples();for(int32 I=0;I<Copy.Num();++I){auto* A=Copy[I].Animation.Get();if(A&&(A->GetName().Equals(Selector,ESearchCase::IgnoreCase)||A->GetPathName().Equals(Selector,ESearchCase::IgnoreCase)||A->GetPackage()->GetName().Equals(Selector,ESearchCase::IgnoreCase))){return I;}}
    Error=FString::Printf(TEXT("No BlendSpace sample matched selector: %s"),*Selector);return INDEX_NONE;
}
bool AxisEdits(UBlendSpace* Asset,const TSharedPtr<FJsonValue>& Values,FWork& Work)
{
    if(Values->Type!=EJson::Array){return true;}
    for(int32 I=0;I<Values->AsArray().Num();++I)
    {
        auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("edit_index"),I);Work.AxisResults.Add(MakeShared<FJsonValueObject>(R));const auto Value=Values->AsArray()[I];
        if(Value->Type!=EJson::Object){Work.RowError(R,TEXT("axis_edits entries must be objects"));continue;}auto Row=Value->AsObject().ToSharedRef();if(!Inventory(Asset,Work)){return false;}
        int32 Index=AxisIndex(Asset,Row);if(Index<0||Index>=3){Work.RowError(R,TEXT("Axis edit must provide index/axis_index 0..2 or axis x/y/z/display name"));continue;}
        FStructProperty* Property=FindFProperty<FStructProperty>(Asset->GetClass(),TEXT("BlendParameters"));
        if(!Property||Property->ArrayDim!=3||Property->Struct!=FBlendParameter::StaticStruct()||Property->GetElementSize()!=static_cast<int32>(sizeof(FBlendParameter))){return Work.Fail(TEXT("axis_reflection_unavailable"),TEXT("Actual reflected BlendParameters struct3/layout does not match the public FBlendParameter SDK type"));}
        UStruct* Owner=Property->GetOwnerStruct();if(!Work.Hold(Owner)){return false;}const FBlendParameter Before=Asset->GetBlendParameter(Index);FBlendParameter After=Before;FString Name;double Grid=0;bool Present=false;
        if(Row->TryGetStringField(TEXT("display_name"),Name)||Row->TryGetStringField(TEXT("name"),Name)){After.DisplayName=Name;}
        if(!Safe(After.DisplayName)||!NumberFloat(Row,TEXT("min"),After.Min,Present)||!NumberFloat(Row,TEXT("max"),After.Max,Present)){Work.RowError(R,TEXT("Axis fields exceed finite bounded values"));continue;}
        if(Row->TryGetNumberField(TEXT("grid_num"),Grid)||Row->TryGetNumberField(TEXT("grid"),Grid)){if(!FMath::IsFinite(Grid)||Grid>MaxGrid||Grid<TNumericLimits<int32>::Min()){Work.RowError(R,TEXT("Axis grid must round within1..256"));continue;}After.GridNum=FMath::Max(1,FMath::RoundToInt(Grid));}
        bool Flag=false;if(Row->TryGetBoolField(TEXT("snap_to_grid"),Flag)){After.bSnapToGrid=Flag;}if(Row->TryGetBoolField(TEXT("wrap_input"),Flag)){After.bWrapInput=Flag;}
        const float NewStep=(After.Max-After.Min)/After.GridNum;if(After.Min>=After.Max||!FMath::IsFinite(NewStep)||NewStep<=0){Work.RowError(R,FString::Printf(TEXT("Axis %d has invalid min/max after edit: min=%f max=%f"),Index,After.Min,After.Max));R->SetObjectField(TEXT("before"),AxisJson(Before,Index));continue;}
        const bool Changed=Before.DisplayName!=After.DisplayName||!FMath::IsNearlyEqual(Before.Min,After.Min)||!FMath::IsNearlyEqual(Before.Max,After.Max)||Before.GridNum!=After.GridNum||Before.bSnapToGrid!=After.bSnapToGrid||Before.bWrapInput!=After.bWrapInput;
        if(!Work.Current()||FindFProperty<FStructProperty>(Asset->GetClass(),TEXT("BlendParameters"))!=Property||Property->GetOwnerStruct()!=Owner){return Work.Fail(TEXT("axis_reflection_changed"),TEXT("Axis reflection descriptor/owner changed before assignment"));}
        if(Changed){auto* Address=Property->ContainerPtrToValuePtr<FBlendParameter>(Asset,Index);*Address=After;Work.Count(TEXT("axis_assignments"));Work.ObservedChange=true;Work.EditReportedChange=true;}
        if(!Work.Current()){return false;}R->SetBoolField(TEXT("success"),true);R->SetNumberField(TEXT("axis_index"),Index);R->SetBoolField(TEXT("changed"),Changed);R->SetObjectField(TEXT("before"),AxisJson(Before,Index));R->SetObjectField(TEXT("after"),AxisJson(Asset->GetBlendParameter(Index),Index));
    }
    return true;
}
bool SampleEdits(UBlendSpace* Asset,const TSharedPtr<FJsonValue>& Values,FWork& Work)
{
    if(Values->Type!=EJson::Array){return true;}
    for(int32 I=0;I<Values->AsArray().Num();++I)
    {
        auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("edit_index"),I);Work.SampleResults.Add(MakeShared<FJsonValueObject>(R));const auto Value=Values->AsArray()[I];if(Value->Type!=EJson::Object){Work.RowError(R,TEXT("sample_edits entries must be objects"));continue;}
        auto Row=Value->AsObject().ToSharedRef();if(!Inventory(Asset,Work)){return false;}FString Error;int32 Index=SampleIndex(Asset,Row,Error);if(Index==INDEX_NONE){Work.RowError(R,Error);continue;}const FBlendSample Before=Asset->GetBlendSample(Index);auto BeforeJson=SampleJson(Before,Index);
        FVector Desired=Before.SampleValue;bool HasValue=false;if(!EditVector(Row,Desired,false,Desired,HasValue,Error)){Work.RowError(R,Error);continue;}
        auto Offset=Row->HasField(TEXT("offset"))?Row->TryGetField(TEXT("offset")):Row->TryGetField(TEXT("delta"));if(Offset.IsValid()){FVector Delta;if(!VectorValue(Offset,Delta)){Work.RowError(R,TEXT("offset/delta must be a vector array or object"));continue;}Desired+=Delta;HasValue=true;}
        if(Desired.ContainsNaN()||FMath::Abs(Desired.GetMax())>TNumericLimits<float>::Max()||FMath::Abs(Desired.GetMin())>TNumericLimits<float>::Max()){Work.RowError(R,TEXT("Desired sample coordinates exceed finite float range"));continue;}
        bool Changed=false;
        if(HasValue&&!Before.SampleValue.Equals(Desired,KINDA_SMALL_NUMBER))
        {
            if(!ExpansionSafe(Asset,Desired,Error)){Work.RowError(R,Error);continue;}
            TArray<TSharedPtr<FJsonValue>> BA,BS;const FString Prior=Snapshot(Asset,Work,BA,BS);if(!Work.Current()){return false;}Work.SDK(TEXT("edit_sample_value_calls"));const bool Returned=Asset->EditSampleValue(Index,Desired);Work.Count(TEXT("edit_sample_value_returns"));
            R->SetBoolField(TEXT("edit_value_returned"),Returned);TArray<TSharedPtr<FJsonValue>> AA,AS;const FString After=Snapshot(Asset,Work,AA,AS);if(!Work.Current()){return false;}Work.ObservedChange|=Prior!=After;
            if(!Returned){Work.RowError(R,FString::Printf(TEXT("Failed to edit BlendSpace sample value at index %d"),Index));continue;}Changed=true;Work.EditReportedChange=true;
        }
        const FBlendSample ExpectedBeforeReplacementLoad=Asset->GetBlendSample(Index);FString Requested;UAnimSequence* Replacement=Animation(Row,{TEXT("replacement_animation"),TEXT("replacement_anim"),TEXT("new_animation")},Requested,Work);if(!Work.Current()){return false;}
        if(!Requested.IsEmpty())
        {
            if(!Replacement){Work.RowError(R,FString::Printf(TEXT("Replacement animation not found or not UAnimSequence: %s"),*Requested));continue;}if(!ClassAllowed(Replacement,Work)||!Inventory(Asset,Work)){return false;}if(!Asset->GetBlendSamples().IsValidIndex(Index)||!(Asset->GetBlendSample(Index)==ExpectedBeforeReplacementLoad)){return Work.Fail(TEXT("selected_sample_changed"),TEXT("Selected sample slot changed during replacement asset load; no wrong-slot write or retry"));}
            Work.SDK(TEXT("replace_sample_animation_calls"));const bool Returned=Asset->ReplaceSampleAnimation(Index,Replacement);Work.Count(TEXT("replace_sample_animation_returns"));if(!Work.Current()||!Inventory(Asset,Work)){return false;}if(!Returned){Work.RowError(R,FString::Printf(TEXT("Failed to replace animation for BlendSpace sample index %d"),Index));continue;}Changed=true;Work.EditReportedChange=true;Work.ObservedChange|=Asset->GetBlendSample(Index).Animation!=Before.Animation;
        }
        R->SetBoolField(TEXT("success"),true);R->SetNumberField(TEXT("sample_index"),Index);R->SetBoolField(TEXT("changed"),Changed);R->SetObjectField(TEXT("before"),BeforeJson);R->SetObjectField(TEXT("after"),SampleJson(Asset->GetBlendSample(Index),Index));
    }
    return true;
}
bool AddSamples(UBlendSpace* Asset,const TSharedPtr<FJsonValue>& Values,FWork& Work)
{
    if(Values->Type!=EJson::Array){return true;}
    for(int32 I=0;I<Values->AsArray().Num();++I)
    {
        auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("add_index"),I);Work.AddResults.Add(MakeShared<FJsonValueObject>(R));auto Value=Values->AsArray()[I];if(Value->Type!=EJson::Object){Work.RowError(R,TEXT("add_samples entries must be objects"));continue;}auto Row=Value->AsObject().ToSharedRef();
        FString Requested;UAnimSequence* A=Animation(Row,{TEXT("animation"),TEXT("animation_path"),TEXT("anim_sequence")},Requested,Work);if(!Work.Current()){return false;}if(!A){Work.RowError(R,Requested.IsEmpty()?TEXT("add_samples entry requires an animation path"):FString::Printf(TEXT("Animation not found or not UAnimSequence: %s"),*Requested));continue;}
        FVector Position;bool Present=false;FString Error;if(!EditVector(Row,FVector::ZeroVector,true,Position,Present,Error)){Work.RowError(R,Error);continue;}if(!ClassAllowed(A,Work)||!Inventory(Asset,Work)){return false;}if(!ExpansionSafe(Asset,Position,Error)){Work.RowError(R,Error);continue;}
        if(Asset->GetBlendSamples().Num()>=MaxRows){return Work.Fail(TEXT("asset_sample_bound"),TEXT("Adding sample would exceed4096"));}
        TArray<TSharedPtr<FJsonValue>> BA,BS;const FString Prior=Snapshot(Asset,Work,BA,BS);Work.SDK(TEXT("add_sample_calls"));const int32 Index=Asset->AddSample(A,Position);Work.Count(TEXT("add_sample_returns"));
        R->SetNumberField(TEXT("returned_sample_index"),Index);TArray<TSharedPtr<FJsonValue>> AA,AS;const FString After=Snapshot(Asset,Work,AA,AS);if(!Work.Current()){return false;}Work.ObservedChange|=Prior!=After;
        if(Index==INDEX_NONE){Work.RowError(R,FString::Printf(TEXT("Failed to add BlendSpace sample animation='%s' value='%s'"),*Requested,*Position.ToString()));continue;}Work.ObservedChange=true;Work.EditReportedChange=true;R->SetBoolField(TEXT("success"),true);R->SetNumberField(TEXT("sample_index"),Index);R->SetStringField(TEXT("animation"),A->GetPathName());R->SetObjectField(TEXT("sample_value"),VectorJson(Position));
    }
    return true;
}
}
Variable::FResult Ensure(const FString& SourceBlendSpace,const FString& VariantName,const FString& TargetRoot,const FString& TargetBlendSpace,const FString& AxisEditsJson,const FString& SampleEditsJson,const FString& AddSamplesJson,bool bOverwriteExisting,bool bSave,bool bDryRun,bool bValidateOnly,bool bAllowNonSample,bool bForceResample)
{
    FWork Work(bSave);if(!IsEditorGameThread()||IsEngineExitRequested()||IsPlaySessionActive()||Running){Work.Fail(TEXT("editor_boundary"),TEXT("Idle nonreentrant Editor game thread required"));return Work.Finish();}TGuardValue<bool> Guard(Running,true);
    for(const FString* Value:{&SourceBlendSpace,&VariantName,&TargetRoot,&TargetBlendSpace}){if(!Safe(*Value)){Work.Fail(TEXT("input_bound"),TEXT("Input text exceeds1Mi UTF16 or contains NUL"));return Work.Finish();}}
    TSharedPtr<FJsonValue> AxisValues,SampleValues,AddValues;if(!Decode(AxisEditsJson,AxisValues)||!Decode(SampleEditsJson,SampleValues)||!Decode(AddSamplesJson,AddValues)){Work.Fail(TEXT("carrier_invalid"),TEXT("Each edit carrier requires finite bounded AnyJSON within1Mi UTF16/depth64/65536slots/4096array rows"));return Work.Finish();}
    FString SourcePath=SourceBlendSpace.TrimStartAndEnd();if(SourcePath.IsEmpty()){Work.Fail(TEXT("source_missing"),TEXT("Missing 'source_blendspace' parameter"));return Work.Finish();}
    Work.Result.Stage=TEXT("load_source_blendspace");Work.Source=Cast<UBlendSpace>(Load(SourcePath,Work));if(!Work.Source){if(Work.Result.Error.IsEmpty()){Work.Fail(TEXT("source_missing"),FString::Printf(TEXT("Source BlendSpace not found or not a UBlendSpace: %s"),*SourcePath));}return Work.Finish();}
    Work.Result.Stage=TEXT("validate_variant_target");
    const FString Variant=Segment(VariantName);FString Root=PackagePath(TargetRoot.IsEmpty()?TEXT("/Game/_MCP_Sample/AnimStudy"):TargetRoot);if(Root.IsEmpty()){Root=TEXT("/Game/_MCP_Sample/AnimStudy");}
    Work.TargetPackage=PackagePath(TargetBlendSpace.IsEmpty()?Root+TEXT("/")+Segment(Work.Source->GetName())+TEXT("_")+Variant:TargetBlendSpace);
    if(!GamePath(Root)||!GamePath(Work.TargetPackage)){Work.Fail(TEXT("target_invalid"),TEXT("Invalid target_root or target_blendspace: writable bounded /Game package required"));return Work.Finish();}
    if(!bAllowNonSample&&!SamplePath(Work.TargetPackage)){Work.Fail(TEXT("target_refused"),TEXT("Refusing target outside /Game/_MCP_Sample, /Game/_MCP_Temp or /Game/CubelessMCPTests unless allow_non_sample=true"));return Work.Finish();}
    Work.InPlace=Work.Source->GetPackage()->GetName().Equals(Work.TargetPackage,ESearchCase::IgnoreCase);if(Work.InPlace&&(!bAllowNonSample||!bForceResample)){Work.Fail(TEXT("source_alias_refused"),TEXT("Refusing to use the source BlendSpace as the target variant unless allow_non_sample=true and force_resample=true"));return Work.Finish();}
    Work.Result.Stage=TEXT("inspect_source_blendspace");
    Work.SourceDirty=Work.Source->GetPackage()->IsDirty();if(!RuntimeGuard(Work.Source,Work)){return Work.Finish();}
    TArray<TSharedPtr<FJsonValue>> SA,SS;const FString SourceSnapshot=Snapshot(Work.Source,Work,SA,SS);if(!Work.Current()){return Work.Finish();}for(int32 I=0;I<3;++I){Work.ProtectedAxes.Add(Work.Source->GetBlendParameter(I));}Work.ProtectedSamples=Work.Source->GetBlendSamples();Work.ProtectSourceValues=true;
    auto L=Work.Legacy;L->SetBoolField(TEXT("dry_run"),bDryRun||bValidateOnly);L->SetBoolField(TEXT("validate_only"),bValidateOnly);L->SetBoolField(TEXT("force_resample"),bForceResample);L->SetBoolField(TEXT("in_place_source_target"),Work.InPlace);L->SetStringField(TEXT("source_blendspace"),Work.Source->GetPathName());L->SetStringField(TEXT("source_package"),Work.Source->GetPackage()->GetName());L->SetStringField(TEXT("source_class"),Work.Source->GetClass()->GetPathName());L->SetStringField(TEXT("target_blendspace"),Work.TargetPackage);L->SetStringField(TEXT("target_root"),Root);L->SetStringField(TEXT("variant_name"),Variant);L->SetBoolField(TEXT("overwrite_existing"),bOverwriteExisting);L->SetArrayField(TEXT("source_axes"),SA);L->SetArrayField(TEXT("source_samples"),SS);
    Work.Result.Stage=TEXT("inspect_target_presence");
    Work.SDK(TEXT("asset_exists_calls"));const bool Exists=UEditorAssetLibrary::DoesAssetExist(Work.TargetPackage);L->SetBoolField(TEXT("target_exists"),Exists);if(!Work.Current()){return Work.Finish();}
    if(bDryRun||bValidateOnly){L->SetStringField(TEXT("runtime_note"),TEXT("Dry run only: no BlendSpace asset was duplicated, edited, or saved."));Work.Result.bSuccess=Work.Result.bPostconditionPassed=true;Work.Result.Stage=TEXT("complete_dry_run");return Work.Finish();}
    Work.Result.Stage=TEXT("prepare_target_blendspace");
    if(Work.InPlace){Work.Target=Work.Source;Work.Reused=true;}
    else
    {
        UObject* Existing=Exists?Load(Work.TargetPackage,Work):nullptr;if(!Work.Current()){return Work.Finish();}
        if(Exists&&(!Existing||Existing==Work.Source||Existing->GetPackage()==Work.Source->GetPackage()||!Existing->GetPackage()->GetName().Equals(Work.TargetPackage,ESearchCase::IgnoreCase))){Work.Fail(TEXT("target_alias_refused"),TEXT("Existing target aliases a protected source or redirects outside its declared package"));return Work.Finish();}
        if(Exists&&bOverwriteExisting)
        {
            if(!OverwriteSafe(Existing,Work)){return Work.Finish();}Work.Result.bMutationAttempted=true;Work.SDK(TEXT("delete_asset_calls"));const bool Deleted=UEditorAssetLibrary::DeleteAsset(Work.TargetPackage);Work.Count(TEXT("delete_asset_returns"));Work.Result.Data->SetBoolField(TEXT("target_delete_returned"),Deleted);Work.ObservedChange|=Deleted;
            // The deleted object is deliberately released from identity admission; source identities remain required.
            Work.Ids.RemoveAll([&](const TUniquePtr<FIdentity>& Id){return Id->Package->GetName().Equals(Work.TargetPackage,ESearchCase::IgnoreCase);});Work.Seen.Remove(Existing);
            if(!Work.Current()){return Work.Finish();}if(!Deleted){Work.Fail(TEXT("delete_failed"),TEXT("Failed to overwrite existing BlendSpace target; no duplicate retry"));return Work.Finish();}Existing=nullptr;
        }
        if(Exists&&!bOverwriteExisting){Work.Target=Cast<UBlendSpace>(Existing);Work.Reused=Work.Target!=nullptr;}
        else{Work.Result.bMutationAttempted=true;Work.SDK(TEXT("duplicate_asset_calls"));Work.Target=Cast<UBlendSpace>(UEditorAssetLibrary::DuplicateAsset(Work.Source->GetPathName(),Work.TargetPackage));Work.Count(TEXT("duplicate_asset_returns"));Work.Created=Work.Target!=nullptr;Work.ObservedChange|=Work.Created;}
        if(!Work.Current()||!Work.Hold(Work.Target)){return Work.Finish();}
    }
    if(!Work.Target){Work.Fail(TEXT("target_preparation_failed"),TEXT("Failed to prepare BlendSpace target asset"));return Work.Finish();}
    if(!Work.Target->GetPackage()->GetName().Equals(Work.TargetPackage,ESearchCase::IgnoreCase)||(!Work.InPlace&&Work.Target->GetPackage()==Work.Source->GetPackage())){Work.Fail(TEXT("target_alias_refused"),TEXT("Prepared target escaped its requested package"));return Work.Finish();}
    if(!RuntimeGuard(Work.Target,Work)){return Work.Finish();}
    TArray<TSharedPtr<FJsonValue>> TA,TS;const FString TargetBefore=Snapshot(Work.Target,Work,TA,TS);L->SetStringField(TEXT("target_class"),Work.Target->GetClass()->GetPathName());L->SetArrayField(TEXT("target_axes_before"),TA);L->SetArrayField(TEXT("target_samples_before"),TS);L->SetBoolField(TEXT("dirty_before"),Work.Target->GetPackage()->IsDirty());
    if(!Work.Current()){return Work.Finish();}Work.Result.bMutationAttempted=true;const bool DirtyBeforeModify=Work.Target->GetPackage()->IsDirty();Work.SDK(TEXT("modify_calls"));Work.Target->Modify();Work.Count(TEXT("modify_returns"));if(!Work.Current()){return Work.Finish();}Work.ObservedChange|=DirtyBeforeModify!=Work.Target->GetPackage()->IsDirty();
    Work.Result.Stage=TEXT("apply_axis_edits");if(!AxisEdits(Work.Target,AxisValues,Work)){return Work.Finish();}
    Work.Result.Stage=TEXT("apply_sample_edits");if(!SampleEdits(Work.Target,SampleValues,Work)){return Work.Finish();}
    Work.Result.Stage=TEXT("add_samples");if(!AddSamples(Work.Target,AddValues,Work)){return Work.Finish();}
    TArray<TSharedPtr<FJsonValue>> EA,ES;const FString Edited=Snapshot(Work.Target,Work,EA,ES);if(!Work.Current()){return Work.Finish();}const bool ValueChanged=TargetBefore!=Edited;Work.ObservedChange|=ValueChanged;const bool Resample=Work.EditReportedChange||bForceResample;L->SetBoolField(TEXT("changed"),Work.EditReportedChange);L->SetBoolField(TEXT("resampled"),false);Work.Result.Data->SetBoolField(TEXT("axes_or_samples_observed_changed"),ValueChanged);Work.Result.Data->SetBoolField(TEXT("failed_call_axis_expansion_does_not_imply_legacy_success"),true);
    if(Resample)
    {
        Work.Result.Stage=TEXT("validate_resample_blendspace");Work.SDK(TEXT("validate_sample_data_calls"));Work.Target->ValidateSampleData();Work.Count(TEXT("validate_sample_data_returns"));if(!Inventory(Work.Target,Work)){return Work.Finish();}
        Work.SDK(TEXT("resample_data_calls"));Work.Target->ResampleData();Work.Count(TEXT("resample_data_returns"));if(!Inventory(Work.Target,Work)){return Work.Finish();}
        TStrongObjectPtr<UPackage> MarkedPackage(Work.Target->GetPackage());const bool DirtyBeforeMark=MarkedPackage->IsDirty();Work.Result.Data->SetStringField(TEXT("mark_dirty_observation_package"),MarkedPackage->GetPathName());Work.Result.Data->SetBoolField(TEXT("target_dirty_before_mark"),DirtyBeforeMark);
        Work.SDK(TEXT("mark_package_dirty_calls"));const bool MarkReturned=Work.Target->MarkPackageDirty();Work.Count(TEXT("mark_package_dirty_returns"));Work.Result.Data->SetBoolField(TEXT("mark_package_dirty_returned"),MarkReturned);const bool CurrentAfterMark=Work.Current();
        if(Live(MarkedPackage.Get())){const bool DirtyAfterMark=MarkedPackage->IsDirty();Work.Result.Data->SetBoolField(TEXT("captured_package_dirty_after_mark"),DirtyAfterMark);Work.ObservedChange|=DirtyBeforeMark!=DirtyAfterMark;}else{Work.Result.Data->SetField(TEXT("captured_package_dirty_after_mark"),MakeShared<FJsonValueNull>());}
        if(!CurrentAfterMark||!Inventory(Work.Target,Work)){return Work.Finish();}Work.SDK(TEXT("post_edit_change_calls"));Work.Target->PostEditChange();Work.Count(TEXT("post_edit_change_returns"));if(!Inventory(Work.Target,Work)){return Work.Finish();}L->SetBoolField(TEXT("resampled"),true);
    }
    Work.Result.Stage=TEXT("save_target_blendspace");auto Save=MakeShared<FJsonObject>();L->SetObjectField(TEXT("save"),Save);Save->SetBoolField(TEXT("requested"),bSave);Save->SetBoolField(TEXT("saved"),false);Save->SetBoolField(TEXT("dirty_before"),Work.Target->GetPackage()->IsDirty());
    if(bSave&&Work.Errors.IsEmpty())
    {
        Work.Result.bSaveAttempted=true;Work.SDK(TEXT("save_loaded_asset_calls"));const bool Saved=UEditorAssetLibrary::SaveLoadedAsset(Work.Target,false);Work.Count(TEXT("save_loaded_asset_returns"));Save->SetBoolField(TEXT("saved"),Saved);Save->SetBoolField(TEXT("legacy_saved_is_sdk_return_only"),true);if(!Work.Current()){return Work.Finish();}
        FString Filename;Work.SDK(TEXT("saved_package_file_queries"));const bool FileExists=FPackageName::DoesPackageExist(Work.TargetPackage,&Filename)&&IFileManager::Get().FileSize(*Filename)>0;const bool Clean=!Work.Target->GetPackage()->IsDirty();Work.Result.bSaved=Saved&&FileExists&&Clean;Save->SetBoolField(TEXT("postcondition_verified"),Work.Result.bSaved);Save->SetBoolField(TEXT("file_exists_nonempty"),FileExists);Save->SetStringField(TEXT("filename"),Filename);Save->SetBoolField(TEXT("reload_verified"),false);if(!Work.Result.bSaved){Work.Errors.Add(MakeShared<FJsonValueString>(TEXT("Requested BlendSpace save failed its SDK-return/file/clean-package postcondition")));}
    }
    else if(bSave){Save->SetBoolField(TEXT("skipped_due_to_errors"),true);Save->SetStringField(TEXT("skip_reason"),TEXT("One or more edits failed; target asset was left dirty instead of saving a partial failure."));}
    Save->SetBoolField(TEXT("dirty_after"),Work.Target->GetPackage()->IsDirty());TArray<TSharedPtr<FJsonValue>> AA,AS;Snapshot(Work.Target,Work,AA,AS);L->SetArrayField(TEXT("target_axes"),AA);L->SetArrayField(TEXT("target_samples"),AS);L->SetBoolField(TEXT("dirty_after"),Work.Target->GetPackage()->IsDirty());if(!RuntimeGuard(Work.Target,Work)){return Work.Finish();}
    if(!Work.InPlace)
    {TArray<TSharedPtr<FJsonValue>> QA,QS;if(Snapshot(Work.Source,Work,QA,QS)!=SourceSnapshot||Work.Source->GetPackage()->IsDirty()!=Work.SourceDirty){Work.Fail(TEXT("protected_source_changed"),TEXT("Clone-mode source axes/samples/dirty state changed; no rollback or retry was requested"));return Work.Finish();}}
    if(!Work.Errors.IsEmpty()){Work.Fail(TEXT("edits_or_save_failed"),TEXT("One or more BlendSpace variant edits or requested save failed"));return Work.Finish();}
    Work.Result.bSuccess=Work.Result.bPostconditionPassed=true;Work.Result.Stage=TEXT("complete");return Work.Finish();
}
}
