// DA original material-function expansion7; public Editor duplication/rewiring only.
#include "Compatibility/UE58/CubelessMaterialFunctionExpansionApi.h"
#include "Compatibility/UE58/CubelessMaterialApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "JsonObjectConverter.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionComposite.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialFunctionInstance.h"
#include "EditorFramework/ThumbnailInfo.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "ThumbnailRendering/SceneThumbnailInfoWithPrimitive.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::MaterialFunctionExpansion
{
namespace
{
using FResult=Variable::FResult;
constexpr int32 MaxNodes=512,MaxSourceNodes=256,MaxSources=32,MaxInputs=512;
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool Safe(const FString& S,int32 Max=2048) { if(S.Len()>Max) { return false; }for(int32 I=0;I<S.Len();++I) { if(!S[I]) { return false; } }return true; }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=false;R.ErrorCode=Code;R.Error=Error;return false; }
struct FMemberInputExpectation
{
    TWeakObjectPtr<UObject> Owner;UObject* Identity=nullptr;int32 Index=INDEX_NONE;bool bMaterialProperty=false;FExpressionInput Expected;
};
// Logical instance and physical base are separate readonly roles; no override baking.
struct FUDObject
{
    UObject* Object=nullptr;UClass* Class=nullptr;UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;bool Dirty=false;
};
struct FUDParent
{
    UMaterialFunctionInterface* Object=nullptr;UMaterialFunctionInterface* Parent=nullptr;UMaterialFunctionInterface* CachedBase=nullptr;UObject* EditorData=nullptr;UObject* Thumbnail=nullptr;
};
struct FUDSource
{
    UMaterialFunctionInterface* Logical=nullptr;UMaterialFunction* Base=nullptr;TArray<FUDParent> Chain;TArray<FUDObject> Objects;
    TArray<TObjectPtr<UMaterialExpression>> Expressions;TSharedPtr<FJsonObject> Before;bool Ready=false;
    TMap<FString,FString> FileHashes;
};
struct FUDConsumer
{
    UObject* Owner=nullptr;int32 Index=INDEX_NONE;bool MaterialProperty=false;FExpressionInput Before;FGuid OutputId,OwnerInputId;
};
struct FUDCall
{
    UMaterialExpressionMaterialFunctionCall* Call=nullptr;UMaterialFunctionInterface* Source=nullptr;
    TArray<FFunctionExpressionInput> Inputs;TArray<FFunctionExpressionOutput> Outputs;TArray<FUDConsumer> Consumers;
};
struct FUDPlainThumbnail
{
    UMaterialFunctionInterface* Source=nullptr;UObject* Thumbnail=nullptr;FUDObject SourceRole,ThumbnailRole;
};
struct FContext
{
    FResult& R;UMaterial* Material=nullptr;bool RestoreFailed=false,WorkReturned=false,Stopped=false,UDObserving=false;
    TMap<UMaterialFunctionInterface*,TSharedPtr<FUDSource>> InstanceSources;
    TMap<UMaterialFunctionInterface*,FUDPlainThumbnail> PlainSourceThumbnails;
    TArray<FUDObject> TargetRoles;TMap<UMaterialExpressionMaterialFunctionCall*,FUDCall> InstanceCalls;UEditorEngine* UDEditor=nullptr;UWorld* UDWorld=nullptr;
    TArray<TStrongObjectPtr<UObject>> Keep;
    TMap<UMaterialFunctionInterface*,TSharedPtr<FJsonObject>> SourceBefore;
    TArray<TSharedPtr<FJsonValue>> Allocations,Retirements,SourcesBefore,SourcesAfter;
    TMap<FExpressionInput*,FMemberInputExpectation> ExpectedInputs;
    TSet<UObject*> RetiredOwners;
    explicit FContext(FResult& In):R(In) {}
};
FContext* Active=nullptr;
void Count(const TCHAR* K) { Active->R.Data->SetNumberField(K,Active->R.Data->GetNumberField(K)+1); }
bool UDPure();
bool UDCurrent();
FExpressionInput* CurrentOwnedInput(UObject* Owner,int32 Index,bool Property);
bool UDStop(const FString& Why)
{
    if(!Active->Stopped) { Active->R.Data->SetStringField(TEXT("instance_source_first_mismatch"),Why);Fail(Active->R,TEXT("material_expansion_instance_source_changed"),Why+TEXT("; actual prefix retained, no further writer/save/retry.")); }
    Active->Stopped=true;Active->RestoreFailed=true;Active->R.bChangeStateKnown=false;Active->R.bPostconditionPassed=false;return false;
}
bool Equal(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type) { return false; }
    switch(A->Type)
    {
    case EJson::Null:return true;case EJson::Boolean:return A->AsBool()==B->AsBool();case EJson::Number:return A->AsNumber()==B->AsNumber();case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
    case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()) { return false; }for(int32 I=0;I<X.Num();++I) { if(!Equal(X[I],Y[I])) { return false; } }return true;}
    case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()) { return false; }for(const auto& P:X->Values) { const auto* V=Y->Values.Find(P.Key);if(!V||!Equal(P.Value,*V)) { return false; } }return true;}default:return false;
    }
}
TArray<TSharedPtr<FJsonValue>> StringArrayToJson(const TArray<FString>& Values)
{ TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& V:Values) { Rows.Add(MakeShared<FJsonValueString>(V)); }return Rows; }
TSharedPtr<FJsonObject> LegacyError(const FString& Message)
{ auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Message);return J; }
bool EngineExpression(UMaterialExpression* E,UObject* Owner)
{ return Loaded(E)&&E->GetOuter()==Owner&&E->GetClass()->GetOutermost()->GetName()==TEXT("/Script/Engine")&&!Cast<UMaterialExpressionComposite>(E)&&FMath::Abs(static_cast<int64>(E->MaterialExpressionEditorX))<=1000000&&FMath::Abs(static_cast<int64>(E->MaterialExpressionEditorY))<=1000000; }
TSharedRef<FJsonObject> InputRow(const FExpressionInput& I)
{ auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("expression"),I.Expression?I.Expression->GetPathName():TEXT(""));J->SetNumberField(TEXT("output_index"),I.OutputIndex);J->SetStringField(TEXT("input_name"),I.InputName.ToString());J->SetNumberField(TEXT("mask"),I.Mask);J->SetNumberField(TEXT("mask_r"),I.MaskR);J->SetNumberField(TEXT("mask_g"),I.MaskG);J->SetNumberField(TEXT("mask_b"),I.MaskB);J->SetNumberField(TEXT("mask_a"),I.MaskA);return J; }
bool FiniteJson(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Values)
{
    if(!V.IsValid()||Depth>32||++Values>100000) { return false; }if(V->Type==EJson::Number) { return FMath::IsFinite(V->AsNumber()); }
    if(V->Type==EJson::Array) { for(const auto& X:V->AsArray()) { if(!FiniteJson(X,Depth+1,Values)) { return false; } } }
    if(V->Type==EJson::Object) { for(const auto& P:V->AsObject()->Values) { if(!FiniteJson(P.Value,Depth+1,Values)) { return false; } } }return true;
}
bool Fields(UObject* O,const TSharedRef<FJsonObject>& Out)
{
    Count(TEXT("serialized_field_capture_calls"));int32 N=0;
    for(TFieldIterator<FProperty> It(O->GetClass());It;++It)
    { auto* P=*It;if(P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated)) { continue; }if(++N>256) { return false; }
      auto V=FJsonObjectConverter::UPropertyToJsonValue(P,P->ContainerPtrToValuePtr<void>(O),0,CPF_Transient|CPF_Deprecated);if(!UDPure()) { return false; }int32 Values=0;if(!FiniteJson(V,0,Values)) { return false; }Out->SetField(P->GetName(),V); }
    FString Text;FJsonSerializer::Serialize(Out,TJsonWriterFactory<>::Create(&Text));return Text.Len()<=2097152;
}
bool Snapshot(UObject* Asset,TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions,const TSharedRef<FJsonObject>& J,UObject* ExpressionOwner=nullptr)
{
    if(!UDPure()) { return false; }if(!ExpressionOwner) { ExpressionOwner=Asset; }TArray<TObjectPtr<UMaterialExpression>> StableExpressions;StableExpressions.Append(Expressions.GetData(),Expressions.Num());
    Count(TEXT("native_graph_capture_calls"));J->SetStringField(TEXT("asset_path"),Asset->GetPathName());J->SetStringField(TEXT("class"),Asset->GetClass()->GetPathName());J->SetBoolField(TEXT("dirty"),Asset->GetOutermost()->IsDirty());
    auto Root=MakeShared<FJsonObject>();J->SetObjectField(TEXT("serialized_asset_fields"),Root);if(!Fields(Asset,Root)||Expressions.Num()>MaxNodes) { return false; }
    TArray<TSharedPtr<FJsonValue>> Nodes;J->SetArrayField(TEXT("nodes"),Nodes);
    for(const auto& Ptr:StableExpressions)
    { if(!UDPure()) { return false; }if(!Ptr.IsResolved()) { return false; }auto* E=Ptr.Get();if(!EngineExpression(E,ExpressionOwner)) { return false; }Active->Keep.Emplace(E);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),E->GetPathName());Row->SetStringField(TEXT("class"),E->GetClass()->GetPathName());Row->SetStringField(TEXT("guid"),E->MaterialExpressionGuid.ToString());
      auto Data=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("serialized_fields"),Data);Nodes.Add(MakeShared<FJsonValueObject>(Row));J->SetArrayField(TEXT("nodes"),Nodes);if(!Fields(E,Data)) { return false; }
      TArray<TSharedPtr<FJsonValue>> Inputs;Row->SetArrayField(TEXT("inputs"),Inputs);int32 CountInputs=0;for(FExpressionInputIterator It(E);It;++It)
      { if(++CountInputs>MaxInputs||!It.Input|| (It.Input->Expression&&!Loaded(It.Input->Expression))) { return false; }Inputs.Add(MakeShared<FJsonValueObject>(InputRow(*It.Input)));Row->SetArrayField(TEXT("inputs"),Inputs); }
    }
    if(auto* M=Cast<UMaterial>(Asset))
    { TArray<TSharedPtr<FJsonValue>> Props;J->SetArrayField(TEXT("material_properties"),Props);for(int32 I=0;I<MP_MAX;++I) { auto* Input=M->GetExpressionInputForProperty(static_cast<EMaterialProperty>(I));if(Input) { if(Input->Expression&&!Loaded(Input->Expression)) { return false; }auto V=InputRow(*Input);V->SetNumberField(TEXT("property"),I);Props.Add(MakeShared<FJsonValueObject>(V));J->SetArrayField(TEXT("material_properties"),Props); } } }
    return true;
}
// Only native already-loaded source roles are observed. Parent/Base are not interchangeable.
bool UDObject(UObject* O,FUDObject& V)
{
    if(!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetOuter())||!Loaded(O->GetOutermost())) { return false; }
    V.Object=O;V.Class=O->GetClass();V.Outer=O->GetOuter();V.Package=O->GetOutermost();V.Path=O->GetPathName();V.Dirty=V.Package->IsDirty();
    Active->Keep.Emplace(O);Active->Keep.Emplace(V.Class);Active->Keep.Emplace(V.Outer);Active->Keep.Emplace(V.Package);return true;
}
bool UDObjectCurrent(const FUDObject& V,bool Source)
{
    return Loaded(V.Object)&&Loaded(V.Class)&&Loaded(V.Outer)&&Loaded(V.Package)&&V.Object->GetClass()==V.Class&&V.Object->GetOuter()==V.Outer&&V.Object->GetOutermost()==V.Package&&V.Object->GetPathName()==V.Path&&(!Source||V.Package->IsDirty()==V.Dirty);
}
bool UDEditorData(UMaterialFunctionInterface* F,UObject*& Data)
{
    auto* P=FindFProperty<FObjectProperty>(UMaterialFunctionInterface::StaticClass(),TEXT("EditorOnlyData"));
    if(!P||P->GetClass()!=FObjectProperty::StaticClass()||P->HasSetterOrGetter()||P->ArrayDim!=1) { return false; }
    const auto Slot=P->GetObjectPtrPropertyValue_InContainer(F);if(!Slot.IsResolved()) { return false; }Data=Slot.Get();
    return Loaded(Data)&&Data->GetOuter()==F&&Data->GetClass()==(F->GetClass()==UMaterialFunction::StaticClass()?UMaterialFunctionEditorOnlyData::StaticClass():UMaterialFunctionInterfaceEditorOnlyData::StaticClass());
}
bool UDThumbnail(UMaterialFunctionInterface* F,UObject*& Thumbnail)
{
    if(!F->ThumbnailInfo.IsResolved()) { return false; }Thumbnail=F->ThumbnailInfo.Get();if(!Thumbnail) { return true; }
    if(!Loaded(Thumbnail)||Thumbnail->GetOuter()!=F) { return false; }auto* Class=Thumbnail->GetClass();
    return Class==UThumbnailInfo::StaticClass()||Class==USceneThumbnailInfo::StaticClass()||Class==USceneThumbnailInfoWithPrimitive::StaticClass();
}
bool UDAdmitPlainThumbnail(UMaterialFunctionInterface* Source,TArray<FString>& Errors)
{
    if(Active->PlainSourceThumbnails.Contains(Source)) { return UDPure(); }
    FUDPlainThumbnail V;V.Source=Source;
    if(!Loaded(Source)||Source->GetClass()!=UMaterialFunction::StaticClass()||!UDObject(Source,V.SourceRole)||!UDThumbnail(Source,V.Thumbnail)) { Errors.Add(TEXT("Resolved nullable native ordinary-function ThumbnailInfo required; custom/unloaded thumbnail references remain valid incomplete scope."));return false; }
    if(V.Thumbnail&&!UDObject(V.Thumbnail,V.ThumbnailRole)) { Errors.Add(TEXT("Loaded source-owned ordinary-function thumbnail role required."));return false; }
    Active->PlainSourceThumbnails.Add(Source,V);Active->R.Data->SetBoolField(TEXT("plain_source_thumbnail_guard_active"),true);return UDPure();
}
bool UDPlainThumbnailsCurrent()
{
    for(const auto& Pair:Active->PlainSourceThumbnails)
    {
        const auto& V=Pair.Value;if(!UDObjectCurrent(V.SourceRole,false)||(V.Thumbnail&&!UDObjectCurrent(V.ThumbnailRole,false))) { return UDStop(TEXT("ordinary source/thumbnail held role")); }
        UObject* Now=nullptr;if(!UDThumbnail(V.Source,Now)||Now!=V.Thumbnail) { return UDStop(TEXT("ordinary source ThumbnailInfo nullable native slot")); }
    }
    return true;
}
bool UDValueOverrides(UMaterialFunctionInstance* I)
{
    // Object-bearing/Atlas overrides are valid wider original inputs, not yet admitted here.
    if(I->ScalarParameterValues.Num()>256||I->VectorParameterValues.Num()>256||I->DoubleVectorParameterValues.Num()>256||I->StaticSwitchParameterValues.Num()>256||I->StaticComponentMaskParameterValues.Num()>256||
       I->TextureParameterValues.Num()||I->TextureCollectionParameterValues.Num()||I->ParameterCollectionParameterValues.Num()||I->FontParameterValues.Num()||I->RuntimeVirtualTextureParameterValues.Num()||I->SparseVolumeTextureParameterValues.Num()) { return false; }
    for(const auto& S:I->ScalarParameterValues) { if(!S.AtlasData.Curve.IsNull()||!S.AtlasData.Atlas.IsNull()) { return false; } }return true;
}
bool UDInputSame(const FExpressionInput& A,const FExpressionInput& B,bool IgnoreIndex=false)
{ return A.Expression==B.Expression&&(IgnoreIndex||A.OutputIndex==B.OutputIndex)&&A.InputName==B.InputName&&A.Mask==B.Mask&&A.MaskR==B.MaskR&&A.MaskG==B.MaskG&&A.MaskB==B.MaskB&&A.MaskA==B.MaskA; }
bool UDOutputSame(const FExpressionOutput& A,const FExpressionOutput& B)
{ return A.OutputName==B.OutputName&&A.Mask==B.Mask&&A.MaskR==B.MaskR&&A.MaskG==B.MaskG&&A.MaskB==B.MaskB&&A.MaskA==B.MaskA; }
bool UDRecordCall(UMaterialExpressionMaterialFunctionCall* Call,UMaterialFunctionInterface* Source,TArray<FString>& Errors)
{
    if(Active->InstanceCalls.Contains(Call)) { return true; }
    if(!Loaded(Call)||Call->GetClass()!=UMaterialExpressionMaterialFunctionCall::StaticClass()||!Call->MaterialFunction.IsResolved()||Call->MaterialFunction.Get()!=Source||Call->FunctionInputs.Num()>MaxInputs||Call->FunctionOutputs.Num()>MaxInputs) { Errors.Add(TEXT("Loaded native instance call and bounded resolved IO cache required."));return false; }
    FUDCall C;C.Call=Call;C.Source=Source;Active->Keep.Emplace(Call);
    TSet<FGuid> IDs;
    for(const auto& Row:Call->FunctionInputs) { if(!Row.ExpressionInput.IsResolved()||!Row.ExpressionInputId.IsValid()||IDs.Contains(Row.ExpressionInputId)) { Errors.Add(TEXT("Resolved unique source input GUID roles required."));return false; }IDs.Add(Row.ExpressionInputId);C.Inputs.Add(Row);Active->Keep.Emplace(Row.ExpressionInput.Get()); }
    IDs.Reset();for(const auto& Row:Call->FunctionOutputs) { if(!Row.ExpressionOutput.IsResolved()||!Row.ExpressionOutputId.IsValid()||IDs.Contains(Row.ExpressionOutputId)) { Errors.Add(TEXT("Resolved unique source output GUID roles required."));return false; }IDs.Add(Row.ExpressionOutputId);C.Outputs.Add(Row);Active->Keep.Emplace(Row.ExpressionOutput.Get()); }
    auto Consumer=[&](UObject* Owner,int32 Index,bool Property,const FExpressionInput& Input)
    { if(Input.Expression!=Call) { return true; }if(!C.Outputs.IsValidIndex(Input.OutputIndex)) { return false; }FUDConsumer V;V.Owner=Owner;V.Index=Index;V.MaterialProperty=Property;V.Before=Input;V.OutputId=C.Outputs[Input.OutputIndex].ExpressionOutputId;if(!Property&&Owner->GetClass()==UMaterialExpressionMaterialFunctionCall::StaticClass()) { auto* OwnerCall=static_cast<UMaterialExpressionMaterialFunctionCall*>(Owner);if(!OwnerCall->FunctionInputs.IsValidIndex(Index)) { return false; }V.OwnerInputId=OwnerCall->FunctionInputs[Index].ExpressionInputId;if(!V.OwnerInputId.IsValid()) { return false; } }C.Consumers.Add(V);return true; };
    for(const auto& P:Active->Material->GetExpressions()) { if(!P.IsResolved()||!Loaded(P.Get())) { return false; }for(FExpressionInputIterator It(P.Get());It;++It) { if(It.Index>=MaxInputs||!It.Input||!Consumer(P.Get(),It.Index,false,*It.Input)) { Errors.Add(TEXT("Finite instance consumer/output GUID role required."));return false; } } }
    for(int32 I=0;I<MP_MAX;++I) { if(auto* Input=Active->Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(I))) { if(!Consumer(Active->Material,I,true,*Input)) { Errors.Add(TEXT("Material property output GUID role required."));return false; } } }
    Active->InstanceCalls.Add(Call,MoveTemp(C));return true;
}
bool UDCallsCurrent()
{
    for(const auto& Pair:Active->InstanceCalls)
    {
        const auto& C=Pair.Value;if(Active->RetiredOwners.Contains(C.Call)) { continue; }const auto* Source=Active->InstanceSources.Find(C.Source);if(!Source) { continue; }
        if(!Loaded(C.Call)||C.Call->GetClass()!=UMaterialExpressionMaterialFunctionCall::StaticClass()||!Active->Material->GetExpressions().Contains(C.Call)||!C.Call->MaterialFunction.IsResolved()||C.Call->MaterialFunction.Get()!=C.Source||C.Call->FunctionInputs.Num()!=C.Inputs.Num()||C.Call->FunctionOutputs.Num()!=C.Outputs.Num()) { return UDStop(TEXT("instance call logical source/IO inventory")); }
        TSet<FGuid> Seen;
        for(auto& Row:C.Call->FunctionInputs)
        {
            const auto* Expected=C.Inputs.FindByPredicate([&](const FFunctionExpressionInput& E){return E.ExpressionInputId==Row.ExpressionInputId;});
            const auto* Plan=Active->ExpectedInputs.Find(&Row.Input);
            if(!Expected||Seen.Contains(Row.ExpressionInputId)||!Row.ExpressionInput.IsResolved()||!UDInputSame(Row.Input,Plan?Plan->Expected:Expected->Input)) { return UDStop(TEXT("instance input GUID/authored connection")); }Seen.Add(Row.ExpressionInputId);
            UMaterialExpressionFunctionInput* Resolved=nullptr;
            for(const auto& P:(*Source)->Expressions) { if(P->GetClass()==UMaterialExpressionFunctionInput::StaticClass()) { auto* I=static_cast<UMaterialExpressionFunctionInput*>(P.Get());if(I->Id==Row.ExpressionInputId) { if(Resolved) { return UDStop(TEXT("ambiguous base input GUID")); }Resolved=I; } } }
            if(!Resolved||(Row.ExpressionInput.Get()&&Row.ExpressionInput.Get()!=Resolved)) { return UDStop(TEXT("instance cached input source-owner/type")); }
        }
        Seen.Reset();for(const auto& Row:C.Call->FunctionOutputs)
        {
            const auto* Expected=C.Outputs.FindByPredicate([&](const FFunctionExpressionOutput& E){return E.ExpressionOutputId==Row.ExpressionOutputId;});
            if(!Expected||Seen.Contains(Row.ExpressionOutputId)||!Row.ExpressionOutput.IsResolved()||!UDOutputSame(Row.Output,Expected->Output)) { return UDStop(TEXT("instance output GUID/metadata")); }Seen.Add(Row.ExpressionOutputId);
            UMaterialExpressionFunctionOutput* Resolved=nullptr;
            for(const auto& P:(*Source)->Expressions) { if(P->GetClass()==UMaterialExpressionFunctionOutput::StaticClass()) { auto* O=static_cast<UMaterialExpressionFunctionOutput*>(P.Get());if(O->Id==Row.ExpressionOutputId) { if(Resolved) { return UDStop(TEXT("ambiguous base output GUID")); }Resolved=O; } } }
            if(!Resolved||(Row.ExpressionOutput.Get()&&Row.ExpressionOutput.Get()!=Resolved)) { return UDStop(TEXT("instance cached output source-owner/type")); }
        }
        // Cache allocation/order may change. Consumers preserve source-output GUID semantics,
        // while the existing explicit input-write plan owns intentional rewiring/restoration.
        for(const auto& V:C.Consumers)
        {
            if(Active->RetiredOwners.Contains(V.Owner)) { continue; }FExpressionInput* Input=nullptr;if(V.OwnerInputId.IsValid()) { if(!Loaded(V.Owner)||V.Owner->GetClass()!=UMaterialExpressionMaterialFunctionCall::StaticClass()) { return UDStop(TEXT("consumer function input owner")); }for(auto& Row:static_cast<UMaterialExpressionMaterialFunctionCall*>(V.Owner)->FunctionInputs) { if(Row.ExpressionInputId==V.OwnerInputId) { if(Input) { return UDStop(TEXT("ambiguous consumer input GUID")); }Input=&Row.Input; } } }else { Input=CurrentOwnedInput(V.Owner,V.Index,V.MaterialProperty); }if(!Input) { return UDStop(TEXT("instance consumer live member")); }
            if(const auto* Plan=Active->ExpectedInputs.Find(Input)) { if(!UDInputSame(*Input,Plan->Expected)) { return UDStop(TEXT("planned instance consumer rewire")); }continue; }
            if(!UDInputSame(*Input,V.Before,true)||!C.Call->FunctionOutputs.IsValidIndex(Input->OutputIndex)||C.Call->FunctionOutputs[Input->OutputIndex].ExpressionOutputId!=V.OutputId) { return UDStop(TEXT("instance consumer source-output GUID")); }
        }
    }
    return true;
}
bool UDPure()
{
    if(!Active||Active->Stopped) { return !Active||!Active->Stopped; }if(!IsEditorGameThread()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested()) { return UDStop(TEXT("source observation unsafe Engine phase")); }if(!UDPlainThumbnailsCurrent()) { return false; }if(Active->InstanceSources.IsEmpty()) { return true; }
    if(!IsEditorGameThread()||GEditor!=Active->UDEditor||!Loaded(Active->UDEditor)||GWorld!=Active->UDWorld||!Loaded(Active->UDWorld)||Active->UDWorld->WorldType!=EWorldType::Editor||Active->UDWorld->bIsTearingDown||IsPlaySessionActive()||IsGarbageCollecting()||UE::IsSavingPackage()||GExitPurge||IsEngineExitRequested()) { return UDStop(TEXT("editor/world context")); }
    for(const auto& V:Active->TargetRoles) { if(!Active->RetiredOwners.Contains(V.Object)&&(!UDObjectCurrent(V,false)||(V.Object!=Active->Material&&!Active->Material->GetExpressions().Contains(static_cast<UMaterialExpression*>(V.Object))))) { return UDStop(TEXT("held target role/membership: ")+V.Path); } }
    for(const auto& Pair:Active->InstanceSources)
    {
        const FUDSource& S=*Pair.Value;
        // All held identities are closed before any typed chain/collection read.
        for(const auto& V:S.Objects) { if(!UDObjectCurrent(V,true)) { return UDStop(TEXT("readonly source role: ")+V.Path); } }
        for(const auto& C:S.Chain)
        {
            UObject* Data=nullptr;if(!UDEditorData(C.Object,Data)||Data!=C.EditorData) { return UDStop(TEXT("source EditorOnlyData slot")); }
            UObject* Thumbnail=nullptr;if(!UDThumbnail(C.Object,Thumbnail)||Thumbnail!=C.Thumbnail) { return UDStop(TEXT("source ThumbnailInfo nullable native role")); }
            if(C.Object->GetClass()==UMaterialFunctionInstance::StaticClass())
            {
                auto* I=static_cast<UMaterialFunctionInstance*>(C.Object);
                if(!I->Parent.IsResolved()||!I->Base.IsResolved()||I->Parent.Get()!=C.Parent||I->Base.Get()!=C.CachedBase||!UDValueOverrides(I)) { return UDStop(TEXT("instance Parent/Base/override admission")); }
            }
        }
        const auto Now=S.Base->GetExpressions();if(Now.Num()!=S.Expressions.Num()) { return UDStop(TEXT("base expression inventory count")); }
        for(int32 N=0;N<Now.Num();++N) { if(!Now[N].IsResolved()||Now[N].Get()!=S.Expressions[N].Get()) { return UDStop(TEXT("base expression inventory slot")); } }
    }
    return UDCallsCurrent();
}
bool UDSnapshot(const FUDSource& S,const TSharedRef<FJsonObject>& J)
{
    if(!UDPure()||!Snapshot(S.Logical,S.Expressions,J,S.Base)) { return false; }
    J->SetStringField(TEXT("expression_owner"),S.Base->GetPathName());J->SetBoolField(TEXT("instance_overrides_baked"),false);
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(const auto& C:S.Chain)
    {
        if(!UDPure()) { return false; }auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("path"),C.Object->GetPathName());Row->SetStringField(TEXT("parent"),C.Parent?C.Parent->GetPathName():TEXT(""));Row->SetStringField(TEXT("cached_base"),C.CachedBase?C.CachedBase->GetPathName():TEXT(""));Row->SetBoolField(TEXT("dirty"),C.Object->GetOutermost()->IsDirty());
        auto Data=MakeShared<FJsonObject>();if(!Fields(C.Object,Data)) { return false; }Row->SetObjectField(TEXT("fields"),Data);
        auto EditorData=MakeShared<FJsonObject>();if(!Fields(C.EditorData,EditorData)) { return false; }Row->SetObjectField(TEXT("editor_fields"),EditorData);Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    J->SetArrayField(TEXT("resolved_parent_chain"),Rows);return UDPure();
}
bool UDCurrent()
{
    if(!UDPure()) { return false; }if(Active->UDObserving||Active->InstanceSources.IsEmpty()) { return true; }
    TGuardValue<bool> Observing(Active->UDObserving,true);
    for(const auto& Pair:Active->InstanceSources)
    {
        const auto& S=*Pair.Value;if(!S.Ready) { continue; }auto Now=MakeShared<FJsonObject>();
        if(!UDSnapshot(S,Now)||!Equal(MakeShared<FJsonValueObject>(S.Before),MakeShared<FJsonValueObject>(Now))) { return UDStop(TEXT("readonly instance/base authored snapshot")); }
    }
    return true;
}
bool UDDisk(FUDSource& S,bool Capture)
{
    if(!UDPure()) { return false; }TSet<UPackage*> Packages;
    for(const auto& V:S.Objects) { Packages.Add(V.Package); }
    for(auto* Package:Packages)
    {
        FString Filename;if(!FPackageName::DoesPackageExist(Package->GetName(),&Filename)) { if(!Capture) { for(const auto& P:S.FileHashes) { if(P.Key.StartsWith(Package->GetName()+TEXT("|"))) { return UDStop(TEXT("source file disappeared")); } } }continue; }
        TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*Filename)||!UDPure()) { return UDStop(TEXT("source file read")); }
        const FString Key=Package->GetName()+TEXT("|")+Filename,Hash=FMD5::HashBytes(Bytes.GetData(),Bytes.Num());
        if(Capture) { S.FileHashes.Add(Key,Hash); }else { const FString* Before=S.FileHashes.Find(Key);if(!Before||*Before!=Hash) { return UDStop(TEXT("readonly source file: ")+Filename); } }
    }
    return UDPure();
}
bool UDEnroll(UMaterialFunctionInterface* Source,TArray<FString>& Errors)
{
    if(Active->InstanceSources.Contains(Source)) { return UDCurrent(); }
    if(Active->WorkReturned||Active->InstanceSources.Num()>=MaxSources) { Errors.Add(TEXT("Previously unobserved instance source after returned writer work or source budget remains valid incomplete scope; no late baseline adopted."));return false; }
    auto S=MakeShared<FUDSource>();S->Logical=Source;TSet<UMaterialFunctionInterface*> Visiting;auto* At=Source;
    for(int32 Depth=0;;++Depth)
    {
        if(Depth>=MaxSources||!Loaded(At)||Visiting.Contains(At)||(At->GetClass()!=UMaterialFunctionInstance::StaticClass()&&At->GetClass()!=UMaterialFunction::StaticClass())) { Errors.Add(TEXT("Instance source requires a finite loaded native Parent chain; null/cycle/custom/unresolved paths remain incomplete."));return false; }
        Visiting.Add(At);FUDObject Role;if(!UDObject(At,Role)||Role.Package==Active->Material->GetOutermost()) { Errors.Add(TEXT("Separate loaded instance/base source packages required; shared target source package remains incomplete."));return false; }
        const FString Package=Role.Package->GetName();if(!Package.StartsWith(TEXT("/Game/"))&&!Package.StartsWith(TEXT("/Engine/"))) { Errors.Add(TEXT("Instance Parent chain content outside /Game or /Engine remains incomplete."));return false; }S->Objects.Add(Role);
        FUDParent C;C.Object=At;if(!UDEditorData(At,C.EditorData)||!UDObject(C.EditorData,Role)) { Errors.Add(TEXT("Loaded resolved native source editor-data role required."));return false; }S->Objects.Add(Role);
        if(!UDThumbnail(At,C.Thumbnail)) { Errors.Add(TEXT("Resolved nullable native source ThumbnailInfo required; custom/unloaded thumbnail references remain valid incomplete scope."));return false; }if(C.Thumbnail) { if(!UDObject(C.Thumbnail,Role)) { return false; }S->Objects.Add(Role); }
        if(At->GetClass()==UMaterialFunction::StaticClass()) { S->Base=static_cast<UMaterialFunction*>(At);S->Chain.Add(C);break; }
        auto* I=static_cast<UMaterialFunctionInstance*>(At);if(!I->Parent.IsResolved()||!I->Base.IsResolved()||!UDValueOverrides(I)) { Errors.Add(TEXT("Resolved Parent/Base and bounded value-only overrides required; object/Atlas override references remain valid incomplete scope."));return false; }
        C.Parent=I->Parent.Get();C.CachedBase=I->Base.Get();if(C.CachedBase) { if(!UDObject(C.CachedBase,Role)) { Errors.Add(TEXT("Loaded cached Base reference required."));return false; }S->Objects.Add(Role); }
        S->Chain.Add(C);At=C.Parent;
    }
    const auto Expressions=S->Base->GetExpressions();if(Expressions.Num()>MaxSourceNodes) { Errors.Add(TEXT("Instance base source-node budget exceeded."));return false; }
    for(const auto& P:Expressions) { if(!P.IsResolved()||!EngineExpression(P.Get(),S->Base)) { Errors.Add(TEXT("Resolved bounded native Engine base expressions required."));return false; }FUDObject V;if(!UDObject(P.Get(),V)) { return false; }S->Objects.Add(V);S->Expressions.Add(P); }
    if(Active->InstanceSources.IsEmpty())
    {
        if(!Loaded(GEditor)||!Loaded(GWorld)||GWorld->WorldType!=EWorldType::Editor) { Errors.Add(TEXT("Loaded Editor/world required for instance expansion."));return false; }Active->UDEditor=GEditor;Active->UDWorld=GWorld;Active->Keep.Emplace(GEditor);Active->Keep.Emplace(GWorld);
        FUDObject V;if(!UDObject(Active->Material,V)) { return false; }Active->TargetRoles.Add(V);
        for(const auto& P:Active->Material->GetExpressions()) { if(!P.IsResolved()||!UDObject(P.Get(),V)) { return false; }Active->TargetRoles.Add(V); }
    }
    Active->InstanceSources.Add(Source,S);if(!UDPure()) { return false; }
    // GetBaseFunction is virtual; exact native resolved chain above makes its recursion finite.
    auto* Returned=Source->GetBaseFunction();Active->Keep.Emplace(Returned);if(!UDPure()||Returned!=S->Base) { return UDStop(TEXT("actual GetBaseFunction return")); }
    S->Before=MakeShared<FJsonObject>();if(!UDSnapshot(*S,S->Before.ToSharedRef())||!UDDisk(*S,true)) { return UDStop(TEXT("instance source initial snapshot")); }S->Ready=true;
    Active->R.Data->SetBoolField(TEXT("instance_source_supported"),true);Active->R.Data->SetBoolField(TEXT("instance_override_baking_performed"),false);return UDCurrent();
}
bool LocateOwnedInput(FExpressionInput* P,TWeakObjectPtr<UObject>& Owner,int32& Index,bool& Property)
{
    for(int32 I=0;I<MP_MAX;++I) { if(Active->Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(I))==P) { Owner=Active->Material;Index=I;Property=true;return true; } }
    for(const auto& Ptr:Active->Material->GetExpressions()) { auto* E=Ptr.Get();if(!Loaded(E)||E->GetOuter()!=Active->Material) { continue; }for(FExpressionInputIterator It(E);It&&It.Index<MaxInputs;++It) { if(It.Input==P) { Owner=E;Index=It.Index;Property=false;return true; } } }return false;
}
FExpressionInput* CurrentOwnedInput(UObject* Owner,int32 Index,bool Property)
{
    if(!Loaded(Owner)) { return nullptr; }if(Property) { return Owner==Active->Material&&Index>=0&&Index<MP_MAX?Active->Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(Index)):nullptr; }
    auto* E=Cast<UMaterialExpression>(Owner);if(!E||E->GetOuter()!=Active->Material||!Active->Material->GetExpressions().Contains(E)||Index<0||Index>=MaxInputs) { return nullptr; }return E->GetInput(Index);
}
void RecordExpectedInput(FExpressionInput& Input)
{
    FMemberInputExpectation E;if(!LocateOwnedInput(&Input,E.Owner,E.Index,E.bMaterialProperty)) { return; }
    E.Identity=E.Owner.Get();E.Expected=Input;Active->ExpectedInputs.Add(&Input,E);Count(TEXT("tracked_member_input_updates"));
    // Record only this actual authored write; cache relocation itself is never a new baseline.
    for(auto& P:Active->InstanceCalls) { if(P.Key==E.Identity&&P.Key->FunctionInputs.IsValidIndex(E.Index)) { const FGuid Id=P.Key->FunctionInputs[E.Index].ExpressionInputId;for(auto& Row:P.Value.Inputs) { if(Row.ExpressionInputId==Id) { Row.Input=Input; } } } }
}
void Allocation(UMaterialExpression* E,const TCHAR* Kind)
{ if(!E) { return; }Active->WorkReturned=true;Active->Keep.Emplace(E);if(!UDCurrent()) { return; }if(!Active->InstanceSources.IsEmpty()) { FUDObject V;if(!EngineExpression(E,Active->Material)||!UDObject(E,V)) { UDStop(TEXT("returned expression identity"));return; }Active->TargetRoles.Add(V); }auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),E->GetPathName());J->SetStringField(TEXT("class"),E->GetClass()->GetPathName());J->SetStringField(TEXT("guid"),E->MaterialExpressionGuid.ToString());J->SetStringField(TEXT("kind"),Kind);Active->Allocations.Add(MakeShared<FJsonValueObject>(J));Active->R.Data->SetArrayField(TEXT("allocated_expression_prefix"),Active->Allocations); }
UMaterialExpression* CreateExpression(UMaterial* M,UClass* C,int32 X,int32 Y)
{ if(!UDCurrent()) { return nullptr; }if(M->GetExpressions().Num()>=MaxNodes) { Count(TEXT("create_budget_refusal_count"));return nullptr; }Active->R.bMutationAttempted=true;Active->R.bChangeStateKnown=false;Count(TEXT("create_expression_calls"));auto* E=UMaterialEditingLibrary::CreateMaterialExpression(M,C,X,Y);Active->Keep.Emplace(E);Count(TEXT("create_expression_returns"));if(!UDCurrent()) { if(E) { Active->WorkReturned=true; }return nullptr; }Allocation(E,TEXT("preview_default"));return Active->Stopped?nullptr:E; }
UMaterialExpression* DuplicateExpression(UMaterial* M,UMaterialFunction* F,UMaterialExpression* E)
{ if(!UDCurrent()) { return nullptr; }if(M->GetExpressions().Num()>=MaxNodes) { Count(TEXT("create_budget_refusal_count"));return nullptr; }Active->R.bMutationAttempted=true;Active->R.bChangeStateKnown=false;Count(TEXT("duplicate_expression_calls"));auto* V=UMaterialEditingLibrary::DuplicateMaterialExpression(M,F,E);Active->Keep.Emplace(V);Count(TEXT("duplicate_expression_returns"));if(!UDCurrent()) { if(V) { Active->WorkReturned=true; }return nullptr; }Allocation(V,TEXT("function_copy"));return Active->Stopped?nullptr:V; }
void DeleteExpression(UMaterial* M,UMaterialExpression* E)
{ if(!UDCurrent()) { return; }const FString Path=E->GetPathName();const bool Owned=E->GetOuter()==M&&M->GetExpressions().Contains(E);if(!Active->InstanceSources.IsEmpty()) { Active->RetiredOwners.Add(E); }Active->R.bMutationAttempted=true;Active->R.bChangeStateKnown=false;Count(TEXT("delete_expression_calls"));UMaterialEditingLibrary::DeleteMaterialExpression(M,E);Count(TEXT("delete_expression_returns"));if(!UDCurrent()) { return; }auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),Path);const bool Absent=!M->GetExpressions().Contains(E);J->SetBoolField(TEXT("serialized_owned_absent"),Absent);Active->Retirements.Add(MakeShared<FJsonValueObject>(J));Active->R.Data->SetArrayField(TEXT("retired_expression_prefix"),Active->Retirements);Active->WorkReturned|=Owned&&Absent;if(Owned&&Absent) { Active->RetiredOwners.Add(E); } }
bool PreviewAcyclic(UMaterialExpressionFunctionInput* Input,TSet<UMaterialExpressionFunctionInput*>& Visiting,int32 Depth)
{ if(!Loaded(Input)||Depth>32||Visiting.Contains(Input)) { return false; }Visiting.Add(Input);bool Good=true;if(auto* Next=Cast<UMaterialExpressionFunctionInput>(Input->Preview.Expression)) { Good=Next->GetOuter()==Input->GetOuter()&&PreviewAcyclic(Next,Visiting,Depth+1); }Visiting.Remove(Input);return Good; }
bool AdmitFunction(UMaterialExpressionMaterialFunctionCall* Call,UMaterialFunctionInterface* Source,TArray<FString>& Errors)
{
    if(Loaded(Source)&&Source->GetClass()==UMaterialFunction::StaticClass()&&!UDAdmitPlainThumbnail(Source,Errors)) { return false; }
    if(Loaded(Source)&&Source->GetClass()==UMaterialFunctionInstance::StaticClass()&&(!UDRecordCall(Call,Source,Errors)||!UDEnroll(Source,Errors))) { return false; }
    if(!UDCurrent()) { return false; }
    const auto* Instance=Active->InstanceSources.Find(Source);UMaterialFunctionInterface* Owner=Instance?(*Instance)->Base:Source;
    if(!Loaded(Source)||(!Instance&&Source->GetClass()!=UMaterialFunction::StaticClass())||Source->GetExpressions().Num()>MaxSourceNodes||Active->Material->GetExpressions().Num()+Source->GetExpressions().Num()*3>MaxNodes)
    { Errors.Add(TEXT("Finite loaded native MaterialFunction or admitted instance/base chain/source-node/created-node budget required; custom/unbounded variants remain incomplete."));return false; }
    const FString Package=Source->GetOutermost()->GetName();if(!Package.StartsWith(TEXT("/Game/"))&&!Package.StartsWith(TEXT("/Engine/"))) { Errors.Add(TEXT("Function source must be runtime /Game or /Engine content; other mounts remain incomplete."));return false; }
    for(const auto& Ptr:Source->GetExpressions())
    { auto* E=Ptr.Get();if(!EngineExpression(E,Owner)) { Errors.Add(TEXT("Loaded bounded native Engine function expressions required; Composite/custom/native extensions remain incomplete."));return false; }
      int32 N=0;for(FExpressionInputIterator It(E);It;++It) { if(++N>MaxInputs|| (It.Input->Expression&&!Loaded(It.Input->Expression))) { Errors.Add(TEXT("Function input observation exceeds finite loaded budget."));return false; } }
      if(auto* I=Cast<UMaterialExpressionFunctionInput>(E)) { const auto& V=I->PreviewValue;TSet<UMaterialExpressionFunctionInput*> Visiting;if(!FMath::IsFinite(V.X)||!FMath::IsFinite(V.Y)||!FMath::IsFinite(V.Z)||!FMath::IsFinite(V.W)||!PreviewAcyclic(I,Visiting,0)) { Errors.Add(TEXT("Finite acyclic preview defaults required before expansion."));return false; } }
    }
    if(!Active->SourceBefore.Contains(Source))
    { if(Active->SourceBefore.Num()>=MaxSources) { Errors.Add(TEXT("Unique source-function budget exceeded."));return false; }Active->Keep.Emplace(Source);auto J=MakeShared<FJsonObject>();Active->SourcesBefore.Add(MakeShared<FJsonValueObject>(J));Active->R.Data->SetArrayField(TEXT("sources_before"),Active->SourcesBefore);if(Instance) { *J=*(*Instance)->Before; }else if(!Snapshot(Source,Source->GetExpressions(),J)) { Errors.Add(TEXT("Function source observation incomplete before allocation."));return false; }Active->SourceBefore.Add(Source,J); }
    return true;
}
void PublishProgress(const TArray<TSharedPtr<FJsonValue>>& Expanded,const TArray<TSharedPtr<FJsonValue>>& Skipped,const TArray<FString>& Errors,int32 Passes,int32 CountExpanded)
{ Active->R.Data->SetArrayField(TEXT("expanded_nodes_prefix"),Expanded);Active->R.Data->SetArrayField(TEXT("skipped_nodes_prefix"),Skipped);Active->R.Data->SetArrayField(TEXT("errors_prefix"),StringArrayToJson(Errors));Active->R.Data->SetNumberField(TEXT("passes_run_prefix"),Passes);Active->R.Data->SetNumberField(TEXT("expanded_count_prefix"),CountExpanded); }
struct FMaterialGraphTarget
{
    UObject* Asset=nullptr;UMaterial* Material=nullptr;
    TConstArrayView<TObjectPtr<UMaterialExpression>> GetExpressions() const { return Material->GetExpressions(); }
    FString GetPathName() const { return Asset->GetPathName(); }
    void MarkChanged() const
    { if(!UDCurrent()) { return; }Count(TEXT("material_modify_calls"));Active->R.bMutationAttempted=true;Active->R.bChangeStateKnown=false;Asset->Modify();if(!UDCurrent()) { return; }Count(TEXT("mark_package_dirty_calls"));Asset->MarkPackageDirty();if(!UDCurrent()) { return; }bool Unconnected=false;
      for(const auto& P:Material->GetExpressions()) { if(auto* C=Cast<UMaterialExpressionCustom>(P.Get())) { for(const auto& I:C->Inputs) { Unconnected|=!I.Input.Expression; } } }
      Active->R.Data->SetBoolField(TEXT("unconnected_custom_post_edit_skipped"),Unconnected);if(!Unconnected) { Active->R.Data->SetBoolField(TEXT("implicit_compile_possible"),true);Count(TEXT("material_post_edit_calls"));Asset->PostEditChange();Count(TEXT("material_post_edit_returns"));UDCurrent(); } }
};
bool IsEngineOrScriptAssetPath(const FString& Path) { return Path.StartsWith(TEXT("/Engine/"))||Path.StartsWith(TEXT("/Script/")); }

FString GetExpressionKey(const UMaterialExpression* Expression)
{
    if (!Expression)
    {
        return FString();
    }

    return FString::Printf(TEXT("path:%s"), *Expression->GetPathName());
}

FString GetExpressionLegacyId(const UMaterialExpression* Expression)
{
    if (!Expression)
    {
        return FString();
    }

    if (Expression->MaterialExpressionGuid.IsValid())
    {
        return Expression->MaterialExpressionGuid.ToString();
    }

    return Expression->GetName();
}

FString GetExpressionId(const UMaterialExpression* Expression)
{
    return GetExpressionKey(Expression);
}

FString GetExpressionGuidString(const UMaterialExpression* Expression)
{
    return Expression ? Expression->MaterialExpressionGuid.ToString() : FString();
}

void AddExpressionReferenceFields(TSharedPtr<FJsonObject> Object, const UMaterialExpression* Expression, const FString& Prefix)
{
    if (!Object.IsValid() || !Expression)
    {
        return;
    }

    Object->SetStringField(Prefix + TEXT("node_id"), GetExpressionId(Expression));
    Object->SetStringField(Prefix + TEXT("node_key"), GetExpressionKey(Expression));
    Object->SetStringField(Prefix + TEXT("legacy_node_id"), GetExpressionLegacyId(Expression));
    Object->SetStringField(Prefix + TEXT("guid"), GetExpressionGuidString(Expression));
    Object->SetStringField(Prefix + TEXT("node_name"), Expression->GetName());
    Object->SetStringField(Prefix + TEXT("object_path"), Expression->GetPathName());
}
bool FindExpressionById(const FMaterialGraphTarget& Target, const FString& NodeId, UMaterialExpression*& OutExpression, FString& OutError)
{
    TArray<UMaterialExpression*> Matches;
    FString Query = NodeId.TrimStartAndEnd();
    FString Prefix;
    FString Value = Query;
    if (Query.Split(TEXT(":"), &Prefix, &Value))
    {
        Prefix = Prefix.ToLower();
    }
    else
    {
        Prefix.Reset();
    }

    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Target.GetExpressions())
    {
        UMaterialExpression* Expression = ExpressionPtr.Get();
        if (!Expression)
        {
            continue;
        }

        bool bMatches = false;
        if (Prefix == TEXT("path"))
        {
            bMatches = Expression->GetPathName().Equals(Value, ESearchCase::IgnoreCase);
        }
        else if (Prefix == TEXT("name"))
        {
            bMatches = Expression->GetName().Equals(Value, ESearchCase::IgnoreCase);
        }
        else if (Prefix == TEXT("guid"))
        {
            bMatches = GetExpressionGuidString(Expression).Equals(Value, ESearchCase::IgnoreCase);
        }
        else
        {
            bMatches =
                GetExpressionId(Expression).Equals(Query, ESearchCase::IgnoreCase) ||
                GetExpressionKey(Expression).Equals(Query, ESearchCase::IgnoreCase) ||
                GetExpressionLegacyId(Expression).Equals(Query, ESearchCase::IgnoreCase) ||
                GetExpressionGuidString(Expression).Equals(Query, ESearchCase::IgnoreCase) ||
                Expression->GetName().Equals(Query, ESearchCase::IgnoreCase) ||
                Expression->GetPathName().Equals(Query, ESearchCase::IgnoreCase);
        }

        if (bMatches)
        {
            Matches.Add(Expression);
        }
    }

    if (Matches.Num() == 1)
    {
        OutExpression = Matches[0];
        return true;
    }
    if (Matches.Num() > 1)
    {
        TArray<FString> MatchIds;
        for (UMaterialExpression* Match : Matches)
        {
            MatchIds.Add(FString::Printf(TEXT("%s guid=%s (%s)"), *GetExpressionKey(Match), *GetExpressionGuidString(Match), *Match->GetClass()->GetName()));
        }
        OutError = FString::Printf(TEXT("Ambiguous material node id '%s'. Matches: %s"), *NodeId, *FString::Join(MatchIds, TEXT(", ")));
        return false;
    }

    OutError = FString::Printf(TEXT("Material node not found: %s"), *NodeId);
    return false;
}
int32 CountMaterialFunctionCalls(UMaterial* Material)
{
    if (!Material)
    {
        return 0;
    }

    int32 Count = 0;
    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Material->GetExpressions())
    {
        if (Cast<UMaterialExpressionMaterialFunctionCall>(ExpressionPtr.Get()))
        {
            ++Count;
        }
    }
    return Count;
}
TArray<FString> GetExpandableFunctionCallNodeIds(
    UMaterial* Material,
    bool bExcludeEngineFunctions,
    TArray<TSharedPtr<FJsonValue>>& OutSkipped)
{
    TArray<FString> NodeIds;
    if (!Material)
    {
        return NodeIds;
    }

    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Material->GetExpressions())
    {
        UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(ExpressionPtr.Get());
        if (!FunctionCall)
        {
            continue;
        }

        UMaterialFunctionInterface* MaterialFunction = FunctionCall->MaterialFunction;
        const FString FunctionPath = MaterialFunction ? MaterialFunction->GetPathName() : FString();
        if (!MaterialFunction)
        {
            TSharedPtr<FJsonObject> SkippedObject = MakeShared<FJsonObject>();
            AddExpressionReferenceFields(SkippedObject, FunctionCall, TEXT(""));
            SkippedObject->SetStringField(TEXT("reason"), TEXT("missing_material_function"));
            OutSkipped.Add(MakeShared<FJsonValueObject>(SkippedObject));
            continue;
        }

        if (bExcludeEngineFunctions && IsEngineOrScriptAssetPath(FunctionPath))
        {
            TSharedPtr<FJsonObject> SkippedObject = MakeShared<FJsonObject>();
            AddExpressionReferenceFields(SkippedObject, FunctionCall, TEXT(""));
            SkippedObject->SetStringField(TEXT("function"), FunctionPath);
            SkippedObject->SetStringField(TEXT("reason"), TEXT("engine_or_script_function"));
            OutSkipped.Add(MakeShared<FJsonValueObject>(SkippedObject));
            continue;
        }

        NodeIds.Add(GetExpressionId(FunctionCall));
    }

    return NodeIds;
}
struct FExpandedOutputReplacement
{
    bool bValid = false;
    FExpressionInput Input;
};

struct FMaterialFunctionExpansionStats
{
    int32 DuplicatedNodeCount = 0;
    int32 PreviewDefaultNodeCount = 0;
    int32 RewiredFunctionInputCount = 0;
    int32 RewiredConsumerCount = 0;
    TArray<FString> CreatedFunctionCallNodeIds;
};

struct FExpressionInputRestore
{
    FExpressionInput* TargetInput = nullptr;
    TWeakObjectPtr<UObject> Owner;
    int32 InputIndex = INDEX_NONE;
    bool bMaterialProperty = false;
    FExpressionInput OriginalInput;
};

struct FMaterialFunctionExpansionBatchState
{
    TArray<UMaterialExpression*> CreatedExpressions;
    TArray<FExpressionInputRestore> InputRestores;
    TArray<UMaterialExpressionMaterialFunctionCall*> FunctionCallsToDelete;
};

void RecordInputRestore(TArray<FExpressionInputRestore>& InputRestores, FExpressionInput& Input)
{
    for (const FExpressionInputRestore& Restore : InputRestores)
    {
        if (Restore.TargetInput == &Input)
        {
            return;
        }
    }

    FExpressionInputRestore Restore;
    if (!LocateOwnedInput(&Input, Restore.Owner, Restore.InputIndex, Restore.bMaterialProperty))
    { Active->RestoreFailed = true; Count(TEXT("input_restore_location_failures")); return; }
    Restore.TargetInput = &Input;
    Restore.OriginalInput = Input;
    InputRestores.Add(Restore);
}

void RestoreExpressionInputs(TArray<FExpressionInputRestore>& InputRestores)
{
    if(!UDCurrent()) { return; }
    for (int32 Index = InputRestores.Num() - 1; Index >= 0; --Index)
    {
        auto& Restore = InputRestores[Index];
        FExpressionInput* Current = CurrentOwnedInput(Restore.Owner.Get(), Restore.InputIndex, Restore.bMaterialProperty);
        if (Current && Current == Restore.TargetInput)
        {
            Count(TEXT("input_restore_write_calls")); *Current = Restore.OriginalInput; RecordExpectedInput(*Current);
        }
        else { Active->RestoreFailed = true; Count(TEXT("input_restore_location_failures")); }
    }

    InputRestores.Reset();
}

void ApplyInputReplacementPreservingTargetMetadata(FExpressionInput& TargetInput, const FExpressionInput& ReplacementInput)
{
    const FName OriginalInputName = TargetInput.InputName;
    const bool bTargetHadMask = TargetInput.Mask != 0;
    const int32 OriginalMask = TargetInput.Mask;
    const int32 OriginalMaskR = TargetInput.MaskR;
    const int32 OriginalMaskG = TargetInput.MaskG;
    const int32 OriginalMaskB = TargetInput.MaskB;
    const int32 OriginalMaskA = TargetInput.MaskA;

    Count(TEXT("input_replacement_apply_calls"));
    TargetInput = ReplacementInput;
    TargetInput.InputName = OriginalInputName;

    if (bTargetHadMask)
    {
        TargetInput.Mask = OriginalMask;
        TargetInput.MaskR = OriginalMaskR;
        TargetInput.MaskG = OriginalMaskG;
        TargetInput.MaskB = OriginalMaskB;
        TargetInput.MaskA = OriginalMaskA;
    }
    RecordExpectedInput(TargetInput);
}

void CleanupCreatedMaterialExpressions(UMaterial* Material, TArray<UMaterialExpression*>& CreatedExpressions);

void RollBackMaterialFunctionExpansionBatch(UMaterial* Material, FMaterialFunctionExpansionBatchState& BatchState)
{
    RestoreExpressionInputs(BatchState.InputRestores);
    CleanupCreatedMaterialExpressions(Material, BatchState.CreatedExpressions);
    BatchState.FunctionCallsToDelete.Reset();
}

void CommitMaterialFunctionExpansionBatch(UMaterial* Material, FMaterialFunctionExpansionBatchState& BatchState)
{
    if (Material)
    {
        for (UMaterialExpressionMaterialFunctionCall* FunctionCall : BatchState.FunctionCallsToDelete)
        {
            if(!UDCurrent()) { return; }
            if (FunctionCall && FunctionCall->GetOuter() == Material)
            {
                DeleteExpression(Material, FunctionCall);
            }
        }
    }

    BatchState.CreatedExpressions.Reset();
    BatchState.InputRestores.Reset();
    BatchState.FunctionCallsToDelete.Reset();
}

const FFunctionExpressionInput* FindFunctionCallInputById(
    const UMaterialExpressionMaterialFunctionCall* FunctionCall,
    const FGuid& InputId)
{
    if (!FunctionCall)
    {
        return nullptr;
    }

    for (const FFunctionExpressionInput& FunctionInput : FunctionCall->FunctionInputs)
    {
        if (FunctionInput.ExpressionInputId == InputId)
        {
            return &FunctionInput;
        }
    }

    return nullptr;
}

void CollectFunctionIoExpressions(
    UMaterialFunctionInterface* MaterialFunction,
    TMap<FGuid, UMaterialExpressionFunctionInput*>& OutInputsById,
    TMap<FGuid, UMaterialExpressionFunctionOutput*>& OutOutputsById)
{
    if (!MaterialFunction)
    {
        return;
    }

    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : MaterialFunction->GetExpressions())
    {
        UMaterialExpression* Expression = ExpressionPtr.Get();
        if (!Expression)
        {
            continue;
        }

        if (UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(Expression))
        {
            OutInputsById.Add(FunctionInput->Id, FunctionInput);
        }
        else if (UMaterialExpressionFunctionOutput* FunctionOutput = Cast<UMaterialExpressionFunctionOutput>(Expression))
        {
            OutOutputsById.Add(FunctionOutput->Id, FunctionOutput);
        }
    }
}

UMaterialExpression* CreatePreviewDefaultExpression(
    UMaterial* Material,
    const UMaterialExpressionFunctionInput* FunctionInput,
    const FVector2D& PastePositionOffset,
    TArray<UMaterialExpression*>& CreatedExpressions,
    FMaterialFunctionExpansionStats& Stats)
{
    if(!UDCurrent()) { return nullptr; }
    if (!Material || !FunctionInput)
    {
        return nullptr;
    }

    const int32 NodeX = FMath::RoundToInt(FunctionInput->MaterialExpressionEditorX + PastePositionOffset.X);
    const int32 NodeY = FMath::RoundToInt(FunctionInput->MaterialExpressionEditorY + PastePositionOffset.Y);

    switch (FunctionInput->InputType.GetValue())
    {
    case FunctionInput_Scalar:
    {
        UMaterialExpressionConstant* Constant = Cast<UMaterialExpressionConstant>(
            CreateExpression(Material, UMaterialExpressionConstant::StaticClass(), NodeX, NodeY));
        if (Constant)
        {
            Constant->R = FunctionInput->PreviewValue.X;
            CreatedExpressions.Add(Constant);
            ++Stats.PreviewDefaultNodeCount;
        }
        return Constant;
    }
    case FunctionInput_Vector2:
    {
        UMaterialExpressionConstant2Vector* Constant = Cast<UMaterialExpressionConstant2Vector>(
            CreateExpression(Material, UMaterialExpressionConstant2Vector::StaticClass(), NodeX, NodeY));
        if (Constant)
        {
            Constant->R = FunctionInput->PreviewValue.X;
            Constant->G = FunctionInput->PreviewValue.Y;
            CreatedExpressions.Add(Constant);
            ++Stats.PreviewDefaultNodeCount;
        }
        return Constant;
    }
    case FunctionInput_Vector3:
    {
        UMaterialExpressionConstant3Vector* Constant = Cast<UMaterialExpressionConstant3Vector>(
            CreateExpression(Material, UMaterialExpressionConstant3Vector::StaticClass(), NodeX, NodeY));
        if (Constant)
        {
            Constant->Constant = FLinearColor(FunctionInput->PreviewValue);
            CreatedExpressions.Add(Constant);
            ++Stats.PreviewDefaultNodeCount;
        }
        return Constant;
    }
    case FunctionInput_Vector4:
    {
        UMaterialExpressionConstant4Vector* Constant = Cast<UMaterialExpressionConstant4Vector>(
            CreateExpression(Material, UMaterialExpressionConstant4Vector::StaticClass(), NodeX, NodeY));
        if (Constant)
        {
            Constant->Constant = FLinearColor(FunctionInput->PreviewValue);
            CreatedExpressions.Add(Constant);
            ++Stats.PreviewDefaultNodeCount;
        }
        return Constant;
    }
    case FunctionInput_MaterialAttributes:
    {
        UMaterialExpressionMakeMaterialAttributes* MakeAttributes = Cast<UMaterialExpressionMakeMaterialAttributes>(
            CreateExpression(Material, UMaterialExpressionMakeMaterialAttributes::StaticClass(), NodeX, NodeY));
        if (!MakeAttributes)
        {
            return nullptr;
        }

        CreatedExpressions.Add(MakeAttributes);
        ++Stats.PreviewDefaultNodeCount;

        UMaterialExpressionConstant3Vector* EmissivePreview = Cast<UMaterialExpressionConstant3Vector>(
            CreateExpression(Material, UMaterialExpressionConstant3Vector::StaticClass(), NodeX - 240, NodeY));
        if(Active->Stopped) { return nullptr; }
        if (EmissivePreview)
        {
            EmissivePreview->Constant = FLinearColor(FunctionInput->PreviewValue);
            MakeAttributes->EmissiveColor.Expression = EmissivePreview;
            MakeAttributes->EmissiveColor.OutputIndex = 0;
            CreatedExpressions.Add(EmissivePreview);
            ++Stats.PreviewDefaultNodeCount;
        }

        return MakeAttributes;
    }
    default:
        return nullptr;
    }
}

bool ResolveCopiedExpressionInput(
    UMaterial* Material,
    const UMaterialExpressionMaterialFunctionCall* FunctionCall,
    const TMap<UMaterialExpression*, UMaterialExpression*>& CopiedExpressions,
    const FVector2D& PastePositionOffset,
    TArray<UMaterialExpression*>& CreatedExpressions,
    TArray<FExpressionInputRestore>& InputRestores,
    FExpressionInput& Input,
    FMaterialFunctionExpansionStats& Stats,
    TArray<FString>& Errors, bool bRecordTargetInput = true);

bool BuildFunctionInputReplacement(
    UMaterial* Material,
    const UMaterialExpressionMaterialFunctionCall* FunctionCall,
    const UMaterialExpressionFunctionInput* FunctionInput,
    const TMap<UMaterialExpression*, UMaterialExpression*>& CopiedExpressions,
    const FVector2D& PastePositionOffset,
    TArray<UMaterialExpression*>& CreatedExpressions,
    TArray<FExpressionInputRestore>& InputRestores,
    FExpressionInput& OutInput,
    FMaterialFunctionExpansionStats& Stats,
    TArray<FString>& Errors)
{
    if (!FunctionInput)
    {
        Errors.Add(TEXT("Function input expression was missing while expanding material function call."));
        return false;
    }

    if (const FFunctionExpressionInput* FunctionCallInput = FindFunctionCallInputById(FunctionCall, FunctionInput->Id))
    {
        if (FunctionCallInput->Input.Expression)
        {
            OutInput = FunctionCallInput->Input;
            return true;
        }
    }

    if (!FunctionInput->bUsePreviewValueAsDefault)
    {
        Errors.Add(FString::Printf(
            TEXT("Function input '%s' is unconnected and does not use preview as default."),
            *FunctionInput->InputName.ToString()));
        return false;
    }

    if (FunctionInput->Preview.Expression)
    {
        OutInput = FunctionInput->Preview;
        if (ResolveCopiedExpressionInput(Material, FunctionCall, CopiedExpressions, PastePositionOffset, CreatedExpressions, InputRestores, OutInput, Stats, Errors, false))
        {
            return true;
        }
    }

    UMaterialExpression* PreviewExpression = CreatePreviewDefaultExpression(Material, FunctionInput, PastePositionOffset, CreatedExpressions, Stats);
    if(Active->Stopped) { return false; }
    if (!PreviewExpression)
    {
        Errors.Add(FString::Printf(
            TEXT("Function input '%s' requires unsupported preview default type %d."),
            *FunctionInput->InputName.ToString(),
            static_cast<int32>(FunctionInput->InputType.GetValue())));
        return false;
    }

    OutInput = FExpressionInput();
    OutInput.Expression = PreviewExpression;
    OutInput.OutputIndex = 0;
    return true;
}

bool ResolveCopiedExpressionInput(
    UMaterial* Material,
    const UMaterialExpressionMaterialFunctionCall* FunctionCall,
    const TMap<UMaterialExpression*, UMaterialExpression*>& CopiedExpressions,
    const FVector2D& PastePositionOffset,
    TArray<UMaterialExpression*>& CreatedExpressions,
    TArray<FExpressionInputRestore>& InputRestores,
    FExpressionInput& Input,
    FMaterialFunctionExpansionStats& Stats,
    TArray<FString>& Errors, bool bRecordTargetInput)
{
    UMaterialExpression* SourceExpression = Input.Expression;
    if (!SourceExpression)
    {
        return true;
    }

    if (UMaterialExpression* const* CopiedExpression = CopiedExpressions.Find(SourceExpression))
    {
        if (bRecordTargetInput) { RecordInputRestore(InputRestores, Input); }
        if (bRecordTargetInput) { Count(TEXT("input_reference_write_calls")); }
        Input.Expression = *CopiedExpression;
        if (bRecordTargetInput) { RecordExpectedInput(Input); }
        return true;
    }

    if (UMaterialExpressionFunctionInput* FunctionInput = Cast<UMaterialExpressionFunctionInput>(SourceExpression))
    {
        FExpressionInput ReplacementInput;
        if (!BuildFunctionInputReplacement(Material, FunctionCall, FunctionInput, CopiedExpressions, PastePositionOffset, CreatedExpressions, InputRestores, ReplacementInput, Stats, Errors))
        {
            return false;
        }

        if (bRecordTargetInput) { RecordInputRestore(InputRestores, Input); }
        ApplyInputReplacementPreservingTargetMetadata(Input, ReplacementInput);
        ++Stats.RewiredFunctionInputCount;
        return true;
    }

    Errors.Add(FString::Printf(
        TEXT("Expression input still points to uncopied function expression '%s'."),
        *SourceExpression->GetPathName()));
    return false;
}

void FindFunctionCallReferencedOutputIndices(
    UMaterial* Material,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    TSet<int32>& OutReferencedOutputIndices)
{
    if (!Material || !FunctionCall)
    {
        return;
    }

    auto VisitInput = [FunctionCall, &OutReferencedOutputIndices](FExpressionInput* Input)
    {
        if (!Input || Input->Expression != FunctionCall)
        {
            return;
        }

        int32 OutputIndex = Input->OutputIndex;
        if (!FunctionCall->FunctionOutputs.IsValidIndex(OutputIndex) && FunctionCall->FunctionOutputs.Num() == 1)
        {
            OutputIndex = 0;
        }
        OutReferencedOutputIndices.Add(OutputIndex);
    };

    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Material->GetExpressions())
    {
        UMaterialExpression* Expression = ExpressionPtr.Get();
        if (!Expression)
        {
            continue;
        }

        for (FExpressionInputIterator It(Expression); It; ++It)
        {
            VisitInput(It.Input);
        }
    }

    for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
    {
        VisitInput(Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(PropertyIndex)));
    }
}

int32 RewireFunctionCallConsumers(
    UMaterial* Material,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    const TArray<FExpandedOutputReplacement>& OutputReplacements,
    TArray<FExpressionInputRestore>& InputRestores)
{
    if (!Material || !FunctionCall)
    {
        return 0;
    }

    int32 RewiredCount = 0;
    auto VisitInput = [FunctionCall, &OutputReplacements, &InputRestores, &RewiredCount](FExpressionInput* Input)
    {
        if (!Input || Input->Expression != FunctionCall)
        {
            return;
        }

        int32 OutputIndex = Input->OutputIndex;
        if (!OutputReplacements.IsValidIndex(OutputIndex) && OutputReplacements.Num() == 1)
        {
            OutputIndex = 0;
        }
        if (OutputReplacements.IsValidIndex(OutputIndex) && OutputReplacements[OutputIndex].bValid)
        {
            RecordInputRestore(InputRestores, *Input);
            ApplyInputReplacementPreservingTargetMetadata(*Input, OutputReplacements[OutputIndex].Input);
            ++RewiredCount;
        }
    };

    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Material->GetExpressions())
    {
        UMaterialExpression* Expression = ExpressionPtr.Get();
        if (!Expression)
        {
            continue;
        }

        for (FExpressionInputIterator It(Expression); It; ++It)
        {
            VisitInput(It.Input);
        }
    }

    for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
    {
        VisitInput(Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(PropertyIndex)));
    }

    return RewiredCount;
}

void CleanupCreatedMaterialExpressions(UMaterial* Material, TArray<UMaterialExpression*>& CreatedExpressions)
{
    if (!Material)
    {
        return;
    }

    for (int32 Index = CreatedExpressions.Num() - 1; Index >= 0; --Index)
    {
        if(!UDCurrent()) { return; }
        if (CreatedExpressions[Index])
        {
            DeleteExpression(Material, CreatedExpressions[Index]);
        }
    }

    CreatedExpressions.Reset();
}

bool ExpandFunctionCallIntoMaterial(
    UMaterial* Material,
    UMaterialExpressionMaterialFunctionCall* FunctionCall,
    FMaterialFunctionExpansionStats& Stats,
    FMaterialFunctionExpansionBatchState& BatchState,
    TArray<FString>& Errors)
{
    if (!Material || !FunctionCall || !FunctionCall->MaterialFunction)
    {
        Errors.Add(TEXT("Missing material, function call, or material function while expanding."));
        return false;
    }

    UMaterialFunctionInterface* MaterialFunction = FunctionCall->MaterialFunction;
    if (!AdmitFunction(FunctionCall, MaterialFunction, Errors)) { return false; }
    TMap<FGuid, UMaterialExpressionFunctionInput*> FunctionInputsById;
    TMap<FGuid, UMaterialExpressionFunctionOutput*> FunctionOutputsById;
    CollectFunctionIoExpressions(MaterialFunction, FunctionInputsById, FunctionOutputsById);

    TArray<UMaterialExpression*> SourceExpressions;
    FVector2D AveragePosition = FVector2D::ZeroVector;
    for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : MaterialFunction->GetExpressions())
    {
        UMaterialExpression* SourceExpression = ExpressionPtr.Get();
        if (!SourceExpression ||
            Cast<UMaterialExpressionFunctionInput>(SourceExpression) ||
            Cast<UMaterialExpressionFunctionOutput>(SourceExpression))
        {
            continue;
        }

        SourceExpressions.Add(SourceExpression);
        AveragePosition.X += SourceExpression->MaterialExpressionEditorX;
        AveragePosition.Y += SourceExpression->MaterialExpressionEditorY;
    }

    if (SourceExpressions.Num() > 0)
    {
        AveragePosition /= SourceExpressions.Num();
    }

    const FVector2D PastePosition(FunctionCall->MaterialExpressionEditorX, FunctionCall->MaterialExpressionEditorY);
    const FVector2D PastePositionOffset = PastePosition - AveragePosition;
    TMap<UMaterialExpression*, UMaterialExpression*> CopiedExpressions;
    TArray<UMaterialExpression*> CreatedExpressions;
    TArray<FExpressionInputRestore> LocalInputRestores;
    const int32 InitialErrorCount = Errors.Num();

    for (UMaterialExpression* SourceExpression : SourceExpressions)
    {
        UMaterialExpression* NewExpression = DuplicateExpression(Material, nullptr, SourceExpression);
        if(Active->Stopped) { return false; }
        if (!NewExpression)
        {
            Errors.Add(FString::Printf(TEXT("Failed to duplicate function expression '%s'."), *SourceExpression->GetPathName()));
            RestoreExpressionInputs(LocalInputRestores);
            CleanupCreatedMaterialExpressions(Material, CreatedExpressions);
            return false;
        }

        CreatedExpressions.Add(NewExpression);
        if (Cast<UMaterialExpressionComposite>(NewExpression))
        {
            Errors.Add(FString::Printf(TEXT("Composite expression '%s' is not supported by material function expansion."), *SourceExpression->GetPathName()));
            RestoreExpressionInputs(LocalInputRestores);
            CleanupCreatedMaterialExpressions(Material, CreatedExpressions);
            return false;
        }

        if(!UDCurrent()) { return false; }Count(TEXT("parameter_guid_update_calls")); NewExpression->UpdateParameterGuid(true, true);if(!UDCurrent()) { return false; }
        NewExpression->MaterialExpressionEditorX = FMath::RoundToInt(SourceExpression->MaterialExpressionEditorX + PastePositionOffset.X);
        NewExpression->MaterialExpressionEditorY = FMath::RoundToInt(SourceExpression->MaterialExpressionEditorY + PastePositionOffset.Y);
        CopiedExpressions.Add(SourceExpression, NewExpression);
        ++Stats.DuplicatedNodeCount;
        if (Cast<UMaterialExpressionMaterialFunctionCall>(NewExpression))
        {
            Stats.CreatedFunctionCallNodeIds.Add(GetExpressionId(NewExpression));
        }
    }

    for (UMaterialExpression* NewExpression : CreatedExpressions)
    {
        if (NewExpression)
        {
            if(!UDCurrent()) { return false; }Count(TEXT("post_copy_node_calls")); NewExpression->PostCopyNode(CreatedExpressions);if(!UDCurrent()) { return false; }
        }
    }

    for (const TPair<UMaterialExpression*, UMaterialExpression*>& Pair : CopiedExpressions)
    {
        UMaterialExpression* NewExpression = Pair.Value;
        if (!NewExpression)
        {
            continue;
        }

        for (FExpressionInputIterator It(NewExpression); It; ++It)
        {
            if (!ResolveCopiedExpressionInput(Material, FunctionCall, CopiedExpressions, PastePositionOffset, CreatedExpressions, LocalInputRestores, *It.Input, Stats, Errors))
            {
                RestoreExpressionInputs(LocalInputRestores);
                CleanupCreatedMaterialExpressions(Material, CreatedExpressions);
                return false;
            }
        }
    }

    TSet<int32> ReferencedOutputIndices;
    FindFunctionCallReferencedOutputIndices(Material, FunctionCall, ReferencedOutputIndices);
    const bool bRequireAllOutputs = ReferencedOutputIndices.Num() == 0;

    TArray<FExpandedOutputReplacement> OutputReplacements;
    OutputReplacements.SetNum(FunctionCall->FunctionOutputs.Num());
    for (int32 OutputIndex = 0; OutputIndex < FunctionCall->FunctionOutputs.Num(); ++OutputIndex)
    {
        if (!bRequireAllOutputs && !ReferencedOutputIndices.Contains(OutputIndex))
        {
            continue;
        }

        const FFunctionExpressionOutput& FunctionCallOutput = FunctionCall->FunctionOutputs[OutputIndex];
        UMaterialExpressionFunctionOutput* FunctionOutput = FunctionCallOutput.ExpressionOutput;
        if (!FunctionOutput)
        {
            FunctionOutput = FunctionOutputsById.FindRef(FunctionCallOutput.ExpressionOutputId);
        }
        if (!FunctionOutput)
        {
            Errors.Add(FString::Printf(TEXT("Function output index %d could not be resolved."), OutputIndex));
            continue;
        }

        if(Active->Stopped) { return false; }FExpressionInput OutputInput = FunctionOutput->A;
        if (!OutputInput.Expression)
        {
            Errors.Add(FString::Printf(TEXT("Function output '%s' is not connected."), *FunctionOutput->OutputName.ToString()));
            continue;
        }

        if (ResolveCopiedExpressionInput(Material, FunctionCall, CopiedExpressions, PastePositionOffset, CreatedExpressions, LocalInputRestores, OutputInput, Stats, Errors, false))
        {
            OutputReplacements[OutputIndex].bValid = true;
            OutputReplacements[OutputIndex].Input = OutputInput;
        }
        if(Active->Stopped) { return false; }
    }

    if(Active->Stopped) { return false; }
    if (Errors.Num() > InitialErrorCount)
    {
        RestoreExpressionInputs(LocalInputRestores);
        CleanupCreatedMaterialExpressions(Material, CreatedExpressions);
        return false;
    }

    for (int32 ReferencedOutputIndex : ReferencedOutputIndices)
    {
        if (!OutputReplacements.IsValidIndex(ReferencedOutputIndex) || !OutputReplacements[ReferencedOutputIndex].bValid)
        {
            Errors.Add(FString::Printf(TEXT("Function call output index %d is referenced but has no valid expanded replacement."), ReferencedOutputIndex));
            RestoreExpressionInputs(LocalInputRestores);
            CleanupCreatedMaterialExpressions(Material, CreatedExpressions);
            return false;
        }
    }

    Stats.RewiredConsumerCount += RewireFunctionCallConsumers(Material, FunctionCall, OutputReplacements, LocalInputRestores);
    BatchState.CreatedExpressions.Append(CreatedExpressions);
    BatchState.InputRestores.Append(LocalInputRestores);
    BatchState.FunctionCallsToDelete.Add(FunctionCall);
    return true;
}
TSharedPtr<FJsonObject> LegacyRun(const TSharedPtr<FJsonObject>& Params)
{
    FString MaterialPath;
    if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
    {
        return LegacyError(TEXT("Missing 'material_path' parameter"));
    }

    FString NodeId;
    Params->TryGetStringField(TEXT("node_id"), NodeId);
    NodeId.TrimStartAndEndInline();

    bool bRecursive = true;
    if (Params->HasField(TEXT("recursive")))
    {
        bRecursive = Params->GetBoolField(TEXT("recursive"));
    }

    bool bExcludeEngineFunctions = true;
    if (Params->HasField(TEXT("exclude_engine_functions")))
    {
        bExcludeEngineFunctions = Params->GetBoolField(TEXT("exclude_engine_functions"));
    }

    bool bSave = true;
    if (Params->HasField(TEXT("save")))
    {
        bSave = Params->GetBoolField(TEXT("save"));
    }

    bool bAllowPartialSave = false;
    if (Params->HasField(TEXT("allow_partial_save")))
    {
        bAllowPartialSave = Params->GetBoolField(TEXT("allow_partial_save"));
    }

    int32 MaxPasses = bRecursive ? 8 : 1;
    double MaxPassesValue = 0.0;
    if (Params->TryGetNumberField(TEXT("max_passes"), MaxPassesValue))
    {
        MaxPasses = FMath::Clamp(static_cast<int32>(MaxPassesValue), 1, 64);
    }

    FMaterialGraphTarget Target;
    FString ErrorMessage;
    Target.Asset = Active->Material; Target.Material = Active->Material;
    if (!Target.Material)
    {
        return LegacyError(ErrorMessage);
    }

    if (!Target.Material)
    {
        return LegacyError(TEXT("expand_material_function_calls currently supports Material assets only"));
    }

    UPackage* Package = Target.Asset ? Target.Asset->GetOutermost() : nullptr;
    const bool bWasDirty = Package ? Package->IsDirty() : false;
    const int32 InitialNodeCount = Target.Material->GetExpressions().Num();
    const int32 InitialFunctionCallCount = CountMaterialFunctionCalls(Target.Material);

    TArray<TSharedPtr<FJsonValue>> Expanded;
    TArray<TSharedPtr<FJsonValue>> Skipped;
    TArray<FString> Errors;
    FMaterialFunctionExpansionBatchState BatchState;
    TSet<FString> CompletedExpansionNodeIds;
    TSet<FString> FailedExpansionNodeIds;
    int32 ExpandedCount = 0;
    int32 PassesRun = 0;
    bool bHitMaxPasses = false;
    const bool bTargetedExpansion = !NodeId.IsEmpty();
    TArray<FString> PendingTargetNodeIds;
    if (bTargetedExpansion)
    {
        PendingTargetNodeIds.Add(NodeId);
    }

    for (int32 PassIndex = 0; PassIndex < MaxPasses; ++PassIndex)
    {
        ++PassesRun;

        TArray<FString> NodeIds;
        if (bTargetedExpansion)
        {
            NodeIds = MoveTemp(PendingTargetNodeIds);
        }
        else
        {
            NodeIds = GetExpandableFunctionCallNodeIds(Target.Material, bExcludeEngineFunctions, Skipped);
        }

        if (NodeIds.Num() == 0)
        {
            break;
        }

        int32 ExpandedThisPass = 0;
        TArray<FString> NextTargetNodeIds;
        for (const FString& CurrentNodeId : NodeIds)
        {
            if (CompletedExpansionNodeIds.Contains(CurrentNodeId) || FailedExpansionNodeIds.Contains(CurrentNodeId))
            {
                continue;
            }

            UMaterialExpression* Expression = nullptr;
            FString FindError;
            if (!FindExpressionById(Target, CurrentNodeId, Expression, FindError))
            {
                Errors.Add(FindError);
                FailedExpansionNodeIds.Add(CurrentNodeId);
                continue;
            }

            UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);
            if (!FunctionCall)
            {
                TSharedPtr<FJsonObject> SkippedObject = MakeShared<FJsonObject>();
                AddExpressionReferenceFields(SkippedObject, Expression, TEXT(""));
                SkippedObject->SetStringField(TEXT("reason"), TEXT("not_a_material_function_call"));
                Skipped.Add(MakeShared<FJsonValueObject>(SkippedObject));
                continue;
            }

            UMaterialFunctionInterface* MaterialFunction = FunctionCall->MaterialFunction;
            const FString FunctionPath = MaterialFunction ? MaterialFunction->GetPathName() : FString();
            if (!MaterialFunction)
            {
                TSharedPtr<FJsonObject> SkippedObject = MakeShared<FJsonObject>();
                AddExpressionReferenceFields(SkippedObject, FunctionCall, TEXT(""));
                SkippedObject->SetStringField(TEXT("reason"), TEXT("missing_material_function"));
                Skipped.Add(MakeShared<FJsonValueObject>(SkippedObject));
                continue;
            }

            if (bExcludeEngineFunctions && IsEngineOrScriptAssetPath(FunctionPath))
            {
                TSharedPtr<FJsonObject> SkippedObject = MakeShared<FJsonObject>();
                AddExpressionReferenceFields(SkippedObject, FunctionCall, TEXT(""));
                SkippedObject->SetStringField(TEXT("function"), FunctionPath);
                SkippedObject->SetStringField(TEXT("reason"), TEXT("engine_or_script_function"));
                Skipped.Add(MakeShared<FJsonValueObject>(SkippedObject));
                continue;
            }

            PublishProgress(Expanded, Skipped, Errors, PassesRun, ExpandedCount);
            const int32 BeforeNodeCount = Target.Material->GetExpressions().Num();
            FMaterialFunctionExpansionStats ExpansionStats;
            if (!ExpandFunctionCallIntoMaterial(Target.Material, FunctionCall, ExpansionStats, BatchState, Errors))
            {
                if(Active->Stopped) { return LegacyError(TEXT("Instance/base source scope lost; actual prefix retained without further observation or cleanup.")); }
                FailedExpansionNodeIds.Add(CurrentNodeId);
                continue;
            }
            const int32 AfterNodeCount = Target.Material->GetExpressions().Num();

            TSharedPtr<FJsonObject> ExpandedObject = MakeShared<FJsonObject>();
            ExpandedObject->SetNumberField(TEXT("pass"), PassIndex + 1);
            ExpandedObject->SetStringField(TEXT("requested_node_id"), CurrentNodeId);
            ExpandedObject->SetStringField(TEXT("function"), FunctionPath);
            ExpandedObject->SetNumberField(TEXT("node_count_before"), BeforeNodeCount);
            ExpandedObject->SetNumberField(TEXT("node_count_after"), AfterNodeCount);
            ExpandedObject->SetNumberField(TEXT("duplicated_node_count"), ExpansionStats.DuplicatedNodeCount);
            ExpandedObject->SetNumberField(TEXT("preview_default_node_count"), ExpansionStats.PreviewDefaultNodeCount);
            ExpandedObject->SetNumberField(TEXT("rewired_function_input_count"), ExpansionStats.RewiredFunctionInputCount);
            ExpandedObject->SetNumberField(TEXT("rewired_consumer_count"), ExpansionStats.RewiredConsumerCount);
            ExpandedObject->SetArrayField(TEXT("created_function_call_node_ids"), StringArrayToJson(ExpansionStats.CreatedFunctionCallNodeIds));
            Expanded.Add(MakeShared<FJsonValueObject>(ExpandedObject));

            if (bTargetedExpansion && bRecursive)
            {
                NextTargetNodeIds.Append(ExpansionStats.CreatedFunctionCallNodeIds);
            }

            ++ExpandedCount;
            ++ExpandedThisPass;
            CompletedExpansionNodeIds.Add(CurrentNodeId);
        }

        if (!bRecursive || ExpandedThisPass == 0)
        {
            break;
        }

        if (bTargetedExpansion)
        {
            PendingTargetNodeIds = MoveTemp(NextTargetNodeIds);
            if (PendingTargetNodeIds.Num() == 0)
            {
                break;
            }
        }

        if (PassIndex + 1 == MaxPasses)
        {
            if (bTargetedExpansion)
            {
                bHitMaxPasses = PendingTargetNodeIds.Num() > 0;
            }
            else
            {
                TArray<TSharedPtr<FJsonValue>> RemainingSkipped;
                TArray<FString> RemainingNodeIds = GetExpandableFunctionCallNodeIds(Target.Material, bExcludeEngineFunctions, RemainingSkipped);
                RemainingNodeIds.RemoveAll([&CompletedExpansionNodeIds, &FailedExpansionNodeIds](const FString& RemainingNodeId)
                {
                    return CompletedExpansionNodeIds.Contains(RemainingNodeId) || FailedExpansionNodeIds.Contains(RemainingNodeId);
                });
                bHitMaxPasses = RemainingNodeIds.Num() > 0;
            }
        }
    }

    PublishProgress(Expanded, Skipped, Errors, PassesRun, ExpandedCount);
    bool bSaved = false;
    bool bRolledBack = false;
    const bool bPartialExpansionWithErrors = ExpandedCount > 0 && Errors.Num() > 0;
    if (Active->RestoreFailed)
    {
        Errors.Add(TEXT("Live input restoration scope was lost; commit/save/dirty reset were not dispatched."));
        Active->R.Data->SetBoolField(TEXT("restoration_failure_write_suppression"),true);
    }
    else if (ExpandedCount > 0)
    {
        if (bPartialExpansionWithErrors && !bAllowPartialSave)
        {
            Count(TEXT("batch_rollback_calls")); RollBackMaterialFunctionExpansionBatch(Target.Material, BatchState);
            bRolledBack = true;
            if (Package && !bWasDirty && !Active->RestoreFailed)
            {
                if(!UDCurrent()) { return LegacyError(TEXT("Instance source scope lost before dirty restoration.")); }Count(TEXT("dirty_flag_restore_calls")); Package->SetDirtyFlag(false);if(!UDCurrent()) { return LegacyError(TEXT("Instance source scope lost after dirty restoration.")); }
            }
        }
        else
        {
            Count(TEXT("batch_commit_calls")); CommitMaterialFunctionExpansionBatch(Target.Material, BatchState);
            Target.MarkChanged();
            if(!UDCurrent()) { return LegacyError(TEXT("Instance/base source scope lost after commit/dirty/PostEdit; save not dispatched.")); }
            if (bSave && (Errors.Num() == 0 || bAllowPartialSave))
            {
                Active->R.bSaveAttempted = true; Count(TEXT("save_loaded_asset_calls"));
                bSaved = UEditorAssetLibrary::SaveLoadedAsset(Target.Asset, false);
                Count(TEXT("save_loaded_asset_returns")); Active->R.bSaved = bSaved;
                Active->R.Data->SetBoolField(TEXT("returned_save_value"), bSaved);
                if(!UDCurrent()) { return LegacyError(TEXT("Instance/base source scope lost after actual save return; no further observation.")); }
            }
        }
    }
    else if (Package && !bWasDirty && Package->IsDirty() && !Active->RestoreFailed)
    {
        if(!UDCurrent()) { return LegacyError(TEXT("Instance source scope lost before dirty restoration.")); }Count(TEXT("dirty_flag_restore_calls")); Package->SetDirtyFlag(false);if(!UDCurrent()) { return LegacyError(TEXT("Instance source scope lost after dirty restoration.")); }
    }

    if(!UDCurrent()) { return LegacyError(TEXT("Instance/base source scope lost; terminal getters suppressed.")); }
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("material"), Target.GetPathName());
    ResultObj->SetBoolField(TEXT("expanded"), ExpandedCount > 0 && !bRolledBack && !Active->RestoreFailed);
    ResultObj->SetNumberField(TEXT("expanded_count"), ExpandedCount);
    ResultObj->SetBoolField(TEXT("rolled_back"), bRolledBack);
    ResultObj->SetNumberField(TEXT("passes_run"), PassesRun);
    ResultObj->SetBoolField(TEXT("hit_max_passes"), bHitMaxPasses);
    ResultObj->SetBoolField(TEXT("exclude_engine_functions"), bExcludeEngineFunctions);
    ResultObj->SetBoolField(TEXT("allow_partial_save"), bAllowPartialSave);
    ResultObj->SetBoolField(TEXT("partial_expansion_with_errors"), ExpandedCount > 0 && Errors.Num() > 0);
    ResultObj->SetNumberField(TEXT("initial_node_count"), InitialNodeCount);
    ResultObj->SetNumberField(TEXT("final_node_count"), Target.Material->GetExpressions().Num());
    ResultObj->SetNumberField(TEXT("initial_function_call_count"), InitialFunctionCallCount);
    ResultObj->SetNumberField(TEXT("final_function_call_count"), CountMaterialFunctionCalls(Target.Material));
    ResultObj->SetArrayField(TEXT("expanded_nodes"), Expanded);
    ResultObj->SetArrayField(TEXT("skipped_nodes"), Skipped);
    ResultObj->SetArrayField(TEXT("errors"), StringArrayToJson(Errors));
    ResultObj->SetBoolField(TEXT("saved"), bSaved);
    ResultObj->SetBoolField(TEXT("was_dirty_before_expand"), bWasDirty);
    ResultObj->SetBoolField(TEXT("dirty_after_expand"), Package ? Package->IsDirty() : false);
    Active->R.Data->SetObjectField(TEXT("legacy_result"), ResultObj);
    return ResultObj;
}
}
Variable::FResult Expand(const FString& Path,const FString& NodeId,bool Recursive,bool ExcludeEngine,int32 MaxPasses,bool Save,bool Partial)
{
    FResult R;R.bSaveRequested=Save;
    for(const TCHAR* K:{TEXT("shared_capture_state_calls"),TEXT("shared_capture_state_returns"),TEXT("transaction_calls"),TEXT("asset_editor_query_calls"),TEXT("native_graph_capture_calls"),TEXT("serialized_field_capture_calls"),TEXT("create_expression_calls"),TEXT("create_expression_returns"),TEXT("duplicate_expression_calls"),TEXT("duplicate_expression_returns"),TEXT("delete_expression_calls"),TEXT("delete_expression_returns"),TEXT("parameter_guid_update_calls"),TEXT("post_copy_node_calls"),TEXT("input_replacement_apply_calls"),TEXT("input_reference_write_calls"),TEXT("input_restore_write_calls"),TEXT("input_restore_location_failures"),TEXT("batch_rollback_calls"),TEXT("batch_commit_calls"),TEXT("dirty_flag_restore_calls"),TEXT("material_modify_calls"),TEXT("mark_package_dirty_calls"),TEXT("material_post_edit_calls"),TEXT("material_post_edit_returns"),TEXT("save_loaded_asset_calls"),TEXT("save_loaded_asset_returns"),TEXT("explicit_compile_calls"),TEXT("create_budget_refusal_count"),TEXT("tracked_member_input_updates")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetStringField(TEXT("scope"),TEXT("Finite owned material graph and serialized fields/source function preservation; native instance logical source/resolved Parent chain and physical base expressions are distinct readonly roles, without override baking; public duplication/cleanup/rewiring. Original no explicit compile/Finish, direct forced save. Implicit editor/cache/compiler/preview/render effects are not zero or shader/cook proof."));
    R.Data->SetStringField(TEXT("changed_scope"),TEXT("Actual returned owned expression allocation/retirement and serialized graph/dirty observations. A successful graph rollback does not erase observed allocation/retirement work."));
    R.Data->SetStringField(TEXT("input_replacement_apply_scope"),TEXT("Explicit replacement-helper calls, including local output/preview projection; input_reference_write/input_restore_write refer only to verified live UObject member inputs."));
    R.Data->SetBoolField(TEXT("atomic_rollback_verified"),false);R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);R.Data->SetBoolField(TEXT("runtime_render_verified"),false);R.Data->SetBoolField(TEXT("full_original_or_runtime_verified"),false);R.Data->SetBoolField(TEXT("implicit_compile_possible"),false);
    if(!IsEditorGameThread()||IsPlaySessionActive()||Active||!Safe(Path)||Path.IsEmpty()||!Safe(NodeId)) { Fail(R,TEXT("material_expansion_input_or_editor_invalid"),TEXT("Bounded path/selector on idle Editor game thread required."));return R; }
    FContext C(R);TGuardValue<FContext*> Guard(Active,&C);Material::FReadMetricsScope Reads;
    auto Params=MakeShared<FJsonObject>();Params->SetStringField(TEXT("material_path"),Path);Params->SetStringField(TEXT("graph_type"),TEXT("material"));
    Material::FState State;FString Error;Count(TEXT("shared_capture_state_calls"));const bool Captured=Material::CaptureState(Material::EOperation::ListNodes,Params,MaxNodes,State,Error);Count(TEXT("shared_capture_state_returns"));
    if(State.Snapshot.IsValid()) { R.Data->SetObjectField(TEXT("shared_preflight_graph"),State.Snapshot); }R.Data->SetObjectField(TEXT("shared_read_metrics"),Reads.ToJson());
    auto* M=Cast<UMaterial>(State.Asset.Get());if(!Captured||!Loaded(M)||M->GetClass()!=UMaterial::StaticClass()||!Material::ValidateMutationTarget(State,Error)||(Save&&!M->GetOutermost()->IsFullyLoaded()))
    { Fail(R,TEXT("material_expansion_target_unsupported"),Error.IsEmpty()?TEXT("Loaded ordinary owned /Game Material and fully loaded save package required."):Error);return R; }
    C.Material=M;C.Keep.Emplace(M);R.Data->SetStringField(TEXT("material"),M->GetPathName());Count(TEXT("asset_editor_query_calls"));auto* Editors=GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if(!Editors||Editors->FindEditorForAsset(M,false)) { Fail(R,TEXT("material_expansion_open_editor_scope_pending"),TEXT("Finite pointer restoration scope requires no open target asset editor; valid interactive variants remain incomplete."));return R; }
    auto Before=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("before"),Before);if(!Snapshot(M,M->GetExpressions(),Before)) { Fail(R,TEXT("material_expansion_before_incomplete"),TEXT("Bounded serialized graph observation incomplete before writer."));return R; }
    Params->SetStringField(TEXT("node_id"),NodeId);Params->SetBoolField(TEXT("recursive"),Recursive);Params->SetBoolField(TEXT("exclude_engine_functions"),ExcludeEngine);Params->SetNumberField(TEXT("max_passes"),MaxPasses);Params->SetBoolField(TEXT("save"),Save);Params->SetBoolField(TEXT("allow_partial_save"),Partial);
    // Starting selected instance calls are enrolled before any transaction/writer callback.
    FMaterialGraphTarget PrimeTarget;PrimeTarget.Asset=M;PrimeTarget.Material=M;TArray<UMaterialExpressionMaterialFunctionCall*> PrimeCalls;
    const FString Query=NodeId.TrimStartAndEnd();
    if(!Query.IsEmpty()) { UMaterialExpression* E=nullptr;FString Ignore;if(FindExpressionById(PrimeTarget,Query,E,Ignore)) { if(auto* Call=Cast<UMaterialExpressionMaterialFunctionCall>(E)) { PrimeCalls.Add(Call); } } }
    else { for(const auto& P:M->GetExpressions()) { if(auto* Call=Cast<UMaterialExpressionMaterialFunctionCall>(P.Get())) { PrimeCalls.Add(Call); } } }
    for(auto* Call:PrimeCalls)
    {
        if(!UDCurrent()) { return R; }if(!Call->MaterialFunction.IsResolved()) { continue; }auto* Source=Call->MaterialFunction.Get();
        if(Loaded(Source)&&Source->GetClass()==UMaterialFunctionInstance::StaticClass()&&(!ExcludeEngine||!IsEngineOrScriptAssetPath(Source->GetPathName()))) { TArray<FString> DeferredErrors;AdmitFunction(Call,Source,DeferredErrors);if(C.Stopped) { return R; } }
    }
    if(!C.InstanceSources.IsEmpty()) { auto PrimeAfter=MakeShared<FJsonObject>();if(!Snapshot(M,M->GetExpressions(),PrimeAfter)||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(PrimeAfter))) { UDStop(TEXT("target changed during instance source enrollment"));return R; } }
    R.Stage=TEXT("expand_function_calls");Count(TEXT("transaction_calls"));auto Transaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessMCP","ExpandMaterialFunctions","Expand Material Function Calls"));
    if(!UDCurrent()) { return R; }const auto Legacy=LegacyRun(Params);
    // Close transaction publication before final observations for this new instance scope.
    if(!C.InstanceSources.IsEmpty()||!C.PlainSourceThumbnails.IsEmpty()) { Transaction.Reset();UDCurrent(); }
    R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    for(const auto& P:Legacy->Values) { R.Data->SetField(P.Key,P.Value); }
    if(C.Stopped) { R.bChanged=C.WorkReturned;R.Data->SetObjectField(TEXT("shared_read_metrics"),Reads.ToJson());return R; }
    R.Stage=TEXT("after_observation");auto After=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("after"),After);R.bChangeStateKnown=false;
    if(!Snapshot(M,M->GetExpressions(),After)) { R.Data->SetObjectField(TEXT("shared_read_metrics"),Reads.ToJson());Fail(R,TEXT("material_expansion_after_incomplete"),TEXT("Current observation incomplete after actual expansion/cleanup/save prefix; no retry."));return R; }
    bool SourcesPreserved=true;for(const auto& P:C.SourceBefore)
    { auto J=MakeShared<FJsonObject>();C.SourcesAfter.Add(MakeShared<FJsonValueObject>(J));R.Data->SetArrayField(TEXT("sources_after"),C.SourcesAfter);const auto* Instance=C.InstanceSources.Find(P.Key);if(!UDCurrent()||!Loaded(P.Key)||(Instance?!UDSnapshot(**Instance,J):!Snapshot(P.Key,P.Key->GetExpressions(),J))) { SourcesPreserved=false;break; }SourcesPreserved&=Equal(MakeShared<FJsonValueObject>(P.Value),MakeShared<FJsonValueObject>(J)); }
    for(auto& P:C.InstanceSources) { if(!UDDisk(*P.Value,false)) { SourcesPreserved=false;break; } }
    if(C.Stopped) { R.bChanged=C.WorkReturned;R.Data->SetObjectField(TEXT("shared_read_metrics"),Reads.ToJson());return R; }
    R.Data->SetBoolField(TEXT("source_functions_preserved"),SourcesPreserved);R.Data->SetBoolField(TEXT("input_restore_location_verified"),!C.RestoreFailed);R.Data->SetObjectField(TEXT("shared_read_metrics"),Reads.ToJson());
    bool GraphValid=true;TSet<UMaterialExpression*> Owned;for(const auto& Ptr:M->GetExpressions()) { Owned.Add(Ptr.Get()); }
    for(const auto& Ptr:M->GetExpressions()) { for(FExpressionInputIterator It(Ptr.Get());It;++It) { GraphValid&=!It.Input->Expression||Owned.Contains(It.Input->Expression); } }
    for(int32 I=0;I<MP_MAX;++I) { auto* Input=M->GetExpressionInputForProperty(static_cast<EMaterialProperty>(I));GraphValid&=!Input||!Input->Expression||Owned.Contains(Input->Expression); }
    bool InputsVerified=true;for(const auto& P:C.ExpectedInputs)
    { if(C.RetiredOwners.Contains(P.Value.Identity)) { continue; }auto* Input=CurrentOwnedInput(P.Value.Owner.Get(),P.Value.Index,P.Value.bMaterialProperty);InputsVerified&=Input&&Input==P.Key&&Equal(MakeShared<FJsonValueObject>(InputRow(*Input)),MakeShared<FJsonValueObject>(InputRow(P.Value.Expected))); }
    R.Data->SetBoolField(TEXT("rewired_member_inputs_verified"),InputsVerified);R.Data->SetNumberField(TEXT("tracked_member_input_count"),C.ExpectedInputs.Num());
    bool Rolled=false;Legacy->TryGetBoolField(TEXT("rolled_back"),Rolled);const bool RollbackGraph=!Rolled||Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(After));R.Data->SetBoolField(TEXT("owned_graph_references_verified"),GraphValid);R.Data->SetBoolField(TEXT("rollback_graph_and_serialized_fields_verified"),RollbackGraph);
    R.bChanged=C.WorkReturned||!Equal(MakeShared<FJsonValueObject>(Before),MakeShared<FJsonValueObject>(After));
    if(!SourcesPreserved||C.RestoreFailed||!GraphValid||!InputsVerified||!RollbackGraph) { Fail(R,TEXT("material_expansion_postcondition_failed"),TEXT("Owned graph/source/rewired input/rollback proof failed; actual prefix retained, no atomic restoration/retry claimed."));return R; }
    R.bChangeStateKnown=true;R.bPostconditionPassed=true;R.Stage=TEXT("complete");R.bSuccess=true;R.Data->SetBoolField(TEXT("legacy_report_returned"),true);R.Data->SetStringField(TEXT("compiler_scope"),TEXT("Original operation has no explicit compile/completion predicate; PostEditChange may schedule work. This operation does not observe or claim shader completion."));
    FString LegacyErrorText;if(Legacy->TryGetStringField(TEXT("error"),LegacyErrorText)) { R.bPostconditionPassed=false;Fail(R,TEXT("material_expansion_legacy_failed"),LegacyErrorText); }
    return R;
}
}
